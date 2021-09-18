// code based on google/protobuf/compiler/cpp/cpp_file.cc
// Integrated with muduo by Shuo Chen.

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <memory>
#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include "cpp_service.h"

#include <stdio.h>
#include <iostream>

namespace google {
namespace protobuf {

namespace compiler {
namespace cpp {

// google/protobuf/compiler/cpp/cpp_helpers.h
string ClassName(const Descriptor* descriptor, bool qualified);
string StripProto(const string& filename);

// 常用分隔符注释。thick是一行“=”，thin是一行“-”。
//extern const char kThickSeparator[];
//extern const char kThinSeparator[];
const char kThickSeparator[] =
    "// ===================================================================\n";
const char kThinSeparator[] =
    "// -------------------------------------------------------------------\n";

namespace muduorpc {

class RpcGenerator : public CodeGenerator
{
 public:
  RpcGenerator() = default;

  // implements CodeGenerator ----------------------------------------
  bool Generate(const FileDescriptor* file,   // file即给定的proto文件元信息，生成到generator_context中
                const string& parameter,
                GeneratorContext* generator_context,
                string* error) const override
  {
    if (file->service_count() == 0)
      return true;
    //std::cout << " ======================================= file->name() : " << file->name() << std::endl;

    string basename = StripProto(file->name()) + ".pb";

    {
      // .pb.h 插入头文件 
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->OpenForInsert(basename + ".h", "includes"));
      io::Printer printer(output.get(), '$');
      printer.Print("#include \"service.h\"\n");
      printer.Print("#include <memory>\n");
    }

    std::vector<std::unique_ptr<ServiceGenerator>> service_generators;

    for (int i = 0; i < file->service_count(); i++) {   // 生成Service
      service_generators.emplace_back(
          new ServiceGenerator(file->service(i), file->name(), i));
    }

    { // .pb.h文件处理
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->OpenForInsert(basename + ".h", "namespace_scope"));
      io::Printer printer(output.get(), '$');

      printer.Print("\n");                            // 换行，并加分隔符注释
      printer.Print(kThickSeparator);
      printer.Print("\n");

      // Generate forward declarations of classes.    // typedef等前置声明
      for (int i = 0; i < file->message_type_count(); i++) {
        printer.Print("typedef     ::std::shared_ptr<$classname$>     $classname$Ptr;\n",
                      "classname", ClassName(file->message_type(i), false));
      }
      printer.Print("typedef     ::std::shared_ptr<class EmptyResponse>   EmptyResponsePtr;\n"); // typedef EmptyResponsePtr

      // Generate service definitions.                // 定义
      for (int i = 0; i < file->service_count(); i++)
      {
        printer.Print("\n");
        printer.Print(kThinSeparator);
        printer.Print("\n");
        service_generators[i]->GenerateDeclarations(&printer);
      }
    }

    { // .pb.cc文件处理
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->OpenForInsert(basename + ".cc", "namespace_scope"));
      io::Printer printer(output.get(), '$');

      // Generate service definitions.                // 实现
      for (int i = 0; i < file->service_count(); i++)
      {
        printer.Print(kThickSeparator);
        printer.Print("\n");
        service_generators[i]->GenerateImplementation(&printer);
        printer.Print("\n");
      }
    }

    return true;
  }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RpcGenerator);
};

}  // namespace muduorpc
}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

namespace gpbc = google::protobuf::compiler;

int main(int argc, char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  gpbc::cpp::muduorpc::RpcGenerator generator;
  return gpbc::PluginMain(argc, argv, &generator);
}
