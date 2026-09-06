// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// A simple example to print out "Hello World!" from a MediaPipe graph.

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/rose/macros.h"
#include <SDL.h>

namespace mediapipe {

absl::Status PrintHelloWorld() {
  // Configures a simple graph, which concatenates 2 PassThroughCalculators.
  CalculatorGraphConfig config =
      ParseTextProtoOrDie<CalculatorGraphConfig>(R"pb(
        input_stream: "in"
        output_stream: "out"
        node {
          calculator: "PassThroughCalculator"
          input_stream: "in"
          output_stream: "out1"
        }
        node {
          calculator: "PassThroughCalculator"
          input_stream: "out1"
          output_stream: "out"
        }
      )pb");

  CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));
  MP_ASSIGN_OR_RETURN(OutputStreamPoller poller,
                      graph.AddOutputStreamPoller("out"));
  MP_RETURN_IF_ERROR(graph.StartRun({}));
  // Give 10 input packets that contains the same string "Hello World!".
  char buf[32];
  for (int i = 0; i < 10; ++i) {
    SDL_snprintf(buf, sizeof(buf), "#%i Hello World!", i);
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
        "in", MakePacket<std::string>(buf).At(Timestamp(i))));
  }
  // Close the input stream "in".
  MP_RETURN_IF_ERROR(graph.CloseInputStream("in"));
  mediapipe::Packet packet;
  // Get the output packets string.
  while (poller.Next(&packet)) {
    SDL_Log("%u, pre packet.Get<std::string>()", SDL_GetTicks());
    ABSL_LOG(WARNING) << packet.Get<std::string>();
  }
  return graph.WaitUntilDone();
}
}  // namespace mediapipe

#include "absl/base/log_severity.h"
#include <absl/log/globals.h>

// int main1(int argc, char** argv) {
//  google::InitGoogleLogging(argv[0]);
// MEDIAPIPE_API int hello_world() {
MEDIAPIPE_API int hello_world() {
  // absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);

  absl::SetMinLogLevel(absl::LogSeverityAtLeast::kWarning);
  absl::SetVLogLevel("my_module", 3);

  ABSL_CHECK(mediapipe::PrintHelloWorld().ok());
  return 0;
}
