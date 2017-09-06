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
#include "instruction_properties.h"

namespace kazan
{
namespace generate_spirv_parser
{
Instruction_properties_descriptors Instruction_properties_descriptors::get() noexcept
{
    using namespace util::string_view_literals;
    typedef Instruction_properties_descriptor::Operand_descriptor Operand_descriptor;
    typedef Operand_descriptor::Integer_literal_size Integer_literal_size;
    static constexpr Instruction_properties_descriptor descriptors[] = {
        {""_sv, "OpSource"_sv, {{}, {Integer_literal_size::always_32bits}, {}, {}}},
        {""_sv, "OpMemberName"_sv, {{}, {Integer_literal_size::always_32bits}, {}}},
        {""_sv,
         "OpLine"_sv,
         {{}, {Integer_literal_size::always_32bits}, {Integer_literal_size::always_32bits}}},
        {""_sv,
         "OpTypeInt"_sv,
         {{}, {Integer_literal_size::always_32bits}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpTypeFloat"_sv, {{}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpTypeVector"_sv, {{}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpTypeMatrix"_sv, {{}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv,
         "OpTypeImage"_sv,
         {{},
          {},
          {},
          {Integer_literal_size::always_32bits},
          {Integer_literal_size::always_32bits},
          {Integer_literal_size::always_32bits},
          {Integer_literal_size::always_32bits},
          {},
          {}}},
        {""_sv, "OpConstantSampler"_sv, {{}, {}, {}, {Integer_literal_size::always_32bits}, {}}},
        {""_sv, "OpArrayLength"_sv, {{}, {}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpMemberDecorate"_sv, {{}, {Integer_literal_size::always_32bits}, {}}},
        {""_sv, "OpGroupMemberDecorate"_sv, {{}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpVectorShuffle"_sv, {{}, {}, {}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpCompositeExtract"_sv, {{}, {}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpCompositeInsert"_sv, {{}, {}, {}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpBranchConditional"_sv, {{}, {}, {}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpLifetimeStart"_sv, {{}, {Integer_literal_size::always_32bits}}},
        {""_sv, "OpLifetimeStop"_sv, {{}, {Integer_literal_size::always_32bits}}},
        {""_sv,
         "OpConstantPipeStorage"_sv,
         {{},
          {},
          {Integer_literal_size::always_32bits},
          {Integer_literal_size::always_32bits},
          {Integer_literal_size::always_32bits}}},
        {""_sv, "OpSwitch"_sv, {{}, {}, {Integer_literal_size::matches_type_of_operand_0}}},

        {"OpenCL.std"_sv, "vloadn"_sv, {{}, {}, {Integer_literal_size::always_32bits}}},
        {"OpenCL.std"_sv, "vload_halfn"_sv, {{}, {}, {Integer_literal_size::always_32bits}}},
        {"OpenCL.std"_sv, "vloada_halfn"_sv, {{}, {}, {Integer_literal_size::always_32bits}}},
    };
    return Instruction_properties_descriptors(descriptors,
                                              sizeof(descriptors) / sizeof(descriptors[0]));
}
}
}
