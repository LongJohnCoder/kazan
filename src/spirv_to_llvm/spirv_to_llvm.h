/*
 * Copyright 2017 Jacob Lifshay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef SPIRV_TO_LLVM_SPIRV_TO_LLVM_H_
#define SPIRV_TO_LLVM_SPIRV_TO_LLVM_H_

#include "spirv/parser.h"
#include <stdexcept>
#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include "llvm_wrapper/llvm_wrapper.h"

namespace vulkan_cpu
{
namespace spirv_to_llvm
{
class Type_descriptor
{
    Type_descriptor(const Type_descriptor &) = delete;
    Type_descriptor &operator=(const Type_descriptor &) = delete;

public:
    Type_descriptor() noexcept = default;
    virtual ~Type_descriptor() = default;
    virtual ::LLVMTypeRef get_or_make_type(bool need_complete_structs) = 0;
    class Recursion_checker;
    class Recursion_checker_state
    {
        friend class Recursion_checker;

    private:
        std::size_t recursion_count = 0;
    };
    class Recursion_checker
    {
        Recursion_checker(const Recursion_checker &) = delete;
        Recursion_checker &operator=(const Recursion_checker &) = delete;

    private:
        Recursion_checker_state &state;

    public:
        explicit Recursion_checker(Recursion_checker_state &state,
                                   std::size_t instruction_start_index)
            : state(state)
        {
            state.recursion_count++;
            if(state.recursion_count > 5)
                throw spirv::Parser_error(instruction_start_index,
                                          instruction_start_index,
                                          "too many recursions making type");
        }
        ~Recursion_checker()
        {
            state.recursion_count--;
        }
        std::size_t get_recursion_count() const noexcept
        {
            return state.recursion_count;
        }
        bool is_nested_recursion() const noexcept
        {
            return get_recursion_count() > 1;
        }
    };
};

class Simple_type_descriptor final : public Type_descriptor
{
private:
    ::LLVMTypeRef type;

public:
    explicit Simple_type_descriptor(::LLVMTypeRef type) noexcept : type(type)
    {
    }
    virtual ::LLVMTypeRef get_or_make_type([[gnu::unused]] bool need_complete_structs) override
    {
        return type;
    }
};

class Pointer_type_descriptor final : public Type_descriptor
{
private:
    std::shared_ptr<Type_descriptor> base;
    std::size_t instruction_start_index;
    ::LLVMTypeRef type;
    Recursion_checker_state recursion_checker_state;

public:
    Pointer_type_descriptor(std::shared_ptr<Type_descriptor> base,
                            std::size_t instruction_start_index) noexcept
        : base(std::move(base)),
          instruction_start_index(instruction_start_index),
          type(nullptr)
    {
    }
    const std::shared_ptr<Type_descriptor> &get_base_type() const noexcept
    {
        return base;
    }
    void set_base_type(std::shared_ptr<Type_descriptor> new_base) noexcept
    {
        assert(!base);
        assert(new_base);
        base = std::move(new_base);
    }
    explicit Pointer_type_descriptor(std::size_t instruction_start_index) noexcept
        : base(nullptr),
          instruction_start_index(instruction_start_index),
          type(nullptr)
    {
    }
    virtual ::LLVMTypeRef get_or_make_type(bool need_complete_structs) override
    {
        if(!type)
        {
            Recursion_checker recursion_checker(recursion_checker_state, instruction_start_index);
            if(!base)
                throw spirv::Parser_error(
                    instruction_start_index,
                    instruction_start_index,
                    "attempting to create type from pointer forward declaration");
            auto base_type = base->get_or_make_type(need_complete_structs);
            constexpr unsigned default_address_space = 0;
            type = ::LLVMPointerType(base_type, default_address_space);
        }
        return type;
    }
};

class Function_type_descriptor final : public Type_descriptor
{
private:
    std::shared_ptr<Type_descriptor> return_type;
    std::vector<std::shared_ptr<Type_descriptor>> args;
    ::LLVMTypeRef type;
    Recursion_checker_state recursion_checker_state;
    std::size_t instruction_start_index;
    bool is_var_arg;

public:
    explicit Function_type_descriptor(std::shared_ptr<Type_descriptor> return_type,
                                      std::vector<std::shared_ptr<Type_descriptor>> args,
                                      std::size_t instruction_start_index,
                                      bool is_var_arg = false) noexcept
        : return_type(std::move(return_type)),
          args(std::move(args)),
          type(nullptr),
          instruction_start_index(instruction_start_index),
          is_var_arg(is_var_arg)
    {
    }
    virtual ::LLVMTypeRef get_or_make_type(bool need_complete_structs) override
    {
        if(!type)
        {
            Recursion_checker recursion_checker(recursion_checker_state, instruction_start_index);
            std::vector<::LLVMTypeRef> llvm_args;
            llvm_args.reserve(args.size());
            auto llvm_return_type = return_type->get_or_make_type(need_complete_structs);
            for(auto &arg : args)
                llvm_args.push_back(arg->get_or_make_type(need_complete_structs));
            type = ::LLVMFunctionType(
                llvm_return_type, llvm_args.data(), llvm_args.size(), is_var_arg);
        }
        return type;
    }
};

class Struct_type_descriptor final : public Type_descriptor
{
public:
    struct Member
    {
        std::vector<spirv::Decoration_with_parameters> decorations;
        std::size_t llvm_member_index = -1;
        std::shared_ptr<Type_descriptor> type;
        explicit Member(std::vector<spirv::Decoration_with_parameters> decorations,
                        std::shared_ptr<Type_descriptor> type) noexcept
            : decorations(std::move(decorations)),
              type(std::move(type))
        {
        }
    };

private:
    std::vector<Member> members;
    util::Enum_map<spirv::Built_in, std::size_t> builtin_members;
    ::LLVMTypeRef type;
    bool is_complete;
    Recursion_checker_state recursion_checker_state;
    std::size_t instruction_start_index;
    void complete_type(bool need_complete_structs);
    void on_add_member(std::size_t added_member_index) noexcept
    {
        assert(!is_complete);
        auto &member = members[added_member_index];
        for(auto &decoration : member.decorations)
            if(decoration.value == spirv::Decoration::built_in)
                builtin_members[util::get<spirv::Decoration_built_in_parameters>(
                                    decoration.parameters)
                                    .built_in] = added_member_index;
    }

public:
    void add_member(Member member)
    {
        members.push_back(std::move(member));
        on_add_member(members.size() - 1);
    }
    const std::vector<Member> &get_members(bool need_llvm_member_indexes)
    {
        if(need_llvm_member_indexes)
            get_or_make_type(true);
        return members;
    }
    explicit Struct_type_descriptor(::LLVMContextRef context,
                                    const char *name,
                                    std::size_t instruction_start_index,
                                    std::vector<Member> members = {})
        : members(std::move(members)),
          builtin_members{},
          type(::LLVMStructCreateNamed(context, name)),
          is_complete(false),
          instruction_start_index(instruction_start_index)
    {
        for(std::size_t member_index = 0; member_index < members.size(); member_index++)
            on_add_member(member_index);
    }
    virtual ::LLVMTypeRef get_or_make_type(bool need_complete_structs) override
    {
        if(need_complete_structs && !is_complete)
        {
            Recursion_checker recursion_checker(recursion_checker_state, instruction_start_index);
            if(recursion_checker.is_nested_recursion())
                need_complete_structs = false;
            complete_type(need_complete_structs);
        }
        return type;
    }
};

struct Converted_module
{
    struct Entry_point
    {
        std::string name;
#warning finish filling in Entry_point
        explicit Entry_point(std::string name) noexcept : name(std::move(name))
        {
        }
    };
    llvm_wrapper::Module module;
    std::vector<Entry_point> entry_points;
    std::shared_ptr<Struct_type_descriptor> io_struct;
    Converted_module() : module(), entry_points()
    {
    }
    explicit Converted_module(llvm_wrapper::Module module,
                              std::vector<Entry_point> entry_points,
                              std::shared_ptr<Struct_type_descriptor> io_struct) noexcept
        : module(std::move(module)),
          entry_points(std::move(entry_points)),
          io_struct(std::move(io_struct))
    {
    }
};

class Spirv_to_llvm;

Converted_module spirv_to_llvm(::LLVMContextRef context,
                               const spirv::Word *shader_words,
                               std::size_t shader_size);
}
}

#endif /* SPIRV_TO_LLVM_SPIRV_TO_LLVM_H_ */
