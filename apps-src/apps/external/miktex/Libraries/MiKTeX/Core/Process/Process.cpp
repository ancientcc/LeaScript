/* process.cpp: executing secondary processes

   Copyright (C) 1996-2021 Christian Schenk

   This file is part of the MiKTeX Core Library.

   The MiKTeX Core Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The MiKTeX Core Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the MiKTeX Core Library; if not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#include "../miktex_Core_config.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <miktex/Core/CommandLineBuilder>
#include <miktex/Core/FileStream>
#include <miktex/Core/Process>
#include <miktex/Trace/Trace>
#include <miktex/Trace/TraceStream>

#include "../miktex_Core_internal.h"

#include "game_latex.hpp"
#include <SDL_log.h>

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Trace;
using namespace MiKTeX::Util;

Process::~Process() noexcept
{
}

void Process::Start(const PathName& fileName, const vector<string>& arguments, FILE* pFileStandardInput, FILE** ppFileStandardInput, FILE** ppFileStandardOutput, FILE** ppFileStandardError, const char* workingDirectory)
{
    VALIDATE(false, null_str);
    // {rose-fix}avoid to use process.
/*
  MIKTEX_ASSERT_STRING_OR_NIL(workingDirectory);

  MIKTEX_ASSERT(pFileStandardInput == nullptr || ppFileStandardInput == nullptr);

  ProcessStartInfo startinfo;

  startinfo.FileName = fileName.ToString();
  startinfo.Arguments = arguments;

  startinfo.StandardInput = pFileStandardInput;

  startinfo.RedirectStandardInput = pFileStandardInput == nullptr && ppFileStandardInput != nullptr;
  startinfo.RedirectStandardOutput = ppFileStandardOutput != nullptr;
  startinfo.RedirectStandardError = ppFileStandardError != nullptr;

  if (workingDirectory != nullptr)
  {
    startinfo.WorkingDirectory = workingDirectory;
  }

  unique_ptr<Process> process(Process::Start(startinfo));

  if (ppFileStandardInput != nullptr)
  {
    *ppFileStandardInput = process->get_StandardInput();
  }

  if (ppFileStandardOutput != nullptr)
  {
    *ppFileStandardOutput = process->get_StandardOutput();
  }

  if (ppFileStandardError != nullptr)
  {
    *ppFileStandardError = process->get_StandardError();
  }

  process->Close();
*/
}

// extern int miktex_pdftex_main(const std::vector<std::string>& args);
extern int miktex_makefmt_main(const std::vector<std::string>& args);
// extern int miktex_makepk_main(const std::vector<std::string>& args);
// extern int miktex_makemf_main(const std::vector<std::string>& args);
// extern int miktex_makebase_main(const std::vector<std::string>& args);
extern int miktex_miktex_main(const std::vector<std::string>& args);
// extern int miktex_xetex_main(const std::vector<std::string>& args);

bool Process::Run(const PathName& fileName, const vector<string>& arguments, function<bool(const void*, size_t)> callback, int* exitCode, MiKTeXException* miktexException, const char* workingDirectory)
{
  MIKTEX_ASSERT_STRING_OR_NIL(workingDirectory);

  auto trace_error = TraceStream::Open(MIKTEX_TRACE_ERROR);
  auto trace_process = TraceStream::Open(MIKTEX_TRACE_PROCESS);

  ProcessStartInfo startinfo;

  startinfo.FileName = fileName.ToString();
  startinfo.Arguments = arguments;

  startinfo.StandardInput = nullptr;
  startinfo.RedirectStandardInput = false;
  startinfo.RedirectStandardOutput = callback ? true : false;
  startinfo.RedirectStandardError = false;

  if (workingDirectory != nullptr)
  {
    startinfo.WorkingDirectory = workingDirectory;
  }

/*
  unique_ptr<Process> process(Process::Start(startinfo));

  if (callback)
  {
    trace_process->WriteLine("core", "start reading the pipe");
    const size_t CHUNK_SIZE = 64;
    char buf[CHUNK_SIZE];
    bool cancelled = false;
    FileStream stdoutStream(process->get_StandardOutput());
    size_t total = 0;
    while (!cancelled && feof(stdoutStream.GetFile()) == 0)
    {
      size_t n = fread(buf, 1, CHUNK_SIZE, stdoutStream.GetFile());
      int err = ferror(stdoutStream.GetFile());
      if (err != 0 && err != EPIPE)
      {
        MIKTEX_FATAL_CRT_ERROR_2("fread", "processFileName", fileName.ToString());
      }
      // pass output to caller
      total += n;
      cancelled = !callback(buf, n);
    }
    trace_process->WriteLine("core", fmt::format("read {0} bytes from the pipe", total));
  }

  // wait for the process to finish
  process->WaitForExit();

  // get the exit code & close process
  ProcessExitStatus exitStatus = process->get_ExitStatus();
  int processExitCode = exitStatus == ProcessExitStatus::Exited ? process->get_ExitCode() : -1;
  MiKTeXException processException;
  bool haveException = process->get_Exception(processException);
  process->Close();
*/
  std::vector<string> arguments2 = arguments;
  // arguments2.erase(arguments2.begin());
  // arguments2.insert(arguments2.begin(), "C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");

  SDL_Log("{android}Process::Run, pre, startinfo.FileName: %s", startinfo.FileName.c_str());

  int code = -1;
  if (startinfo.FileName.find("miktex-makefmt.exe") != std::string::npos) {
      code = miktex_makefmt_main(arguments2);

  } else if (startinfo.FileName.find("miktex-pdftex.exe") != std::string::npos) {
      VALIDATE(false, null_str);
#ifdef pdfTeX
      // code = miktex_pdftex_main(arguments2);
#else
      VALIDATE(false, null_str);
#endif

  } else if (startinfo.FileName.find("miktex.exe") != std::string::npos) {
      code = miktex_miktex_main(arguments2);

  } else if (startinfo.FileName.find("miktex-xetex.exe") != std::string::npos) {
      VALIDATE(false, null_str);
      // code = miktex_xetex_main(arguments2);

  } else if (startinfo.FileName.find("miktex-luahbtex.exe") != std::string::npos) {
      code = miktex_luahbtex_main(arguments2, nullptr);

  } /* else if (startinfo.FileName.find("miktex-makepk.exe") != std::string::npos) {
      code = miktex_makepk_main(arguments2);

  } else if (startinfo.FileName.find("miktex-mf.exe") != std::string::npos) {
      code = miktex_makemf_main(arguments2);

  } else if (startinfo.FileName.find("miktex-makebase.exe") != std::string::npos) {
      code = miktex_makebase_main(arguments2);

  } */ else {
      MIKTEX_EXPECT(false);
  }

  SDL_Log("{android}Process::Run, post, startinfo.FileName: %s, code: %i", startinfo.FileName.c_str(), code);

  ProcessExitStatus exitStatus = ProcessExitStatus::Exited;
  int processExitCode = exitStatus == ProcessExitStatus::Exited ? code: -1;
  MiKTeXException processException;
  bool haveException = false;
  if (processExitCode != 0 && miktexException != nullptr)
  {
    if (haveException)
    {
      *miktexException = processException;
    }
    else
    {
      *miktexException = MiKTeXException(
        fileName.GetFileName().ToDisplayString(),
        T_("The executed process did not succeed."),
        MiKTeXException::KVMAP(
          "fileName", fileName.ToDisplayString(),
          "exitCode", std::to_string(processExitCode)),
        SourceLocation());
    }
  }

  if (exitCode != nullptr)
  {
    *exitCode = processExitCode;
    return exitStatus == ProcessExitStatus::Exited;
  }
  else if (processExitCode == 0)
  {
    return true;
  }
  else
  {
    if (exitStatus == ProcessExitStatus::Exited)
    {
      trace_error->WriteLine("core", TraceLevel::Error, fmt::format("{0} returned with exit code {1}", Q_(fileName), processExitCode));
    }
    else if (exitStatus == ProcessExitStatus::Signaled)
    {
      trace_error->WriteLine("core", TraceLevel::Error, fmt::format("{0} was killed by a signal", Q_(fileName)));
    }
    else
    {
      trace_error->WriteLine("core", TraceLevel::Error, fmt::format("{0} exited abnormally", Q_(fileName)));
    }
    return false;
  }
}

bool Process::Run(const PathName& fileName, const vector<string>& arguments, IRunProcessCallback* callback, int* exitCode, MiKTeXException* miktexException, const char* workingDirectory)
{
  function<bool(const void*, size_t)> fcallback;
  if (callback != nullptr)
  {
    fcallback = [callback](const void* output, size_t n) { return callback->OnProcessOutput(output, n); };
  }
  return Run(fileName, arguments, fcallback, exitCode, miktexException, workingDirectory);
}

void Process::Run(const PathName& fileName, const vector<string>& arguments)
{
  Process::Run(fileName, arguments, nullptr);
}

void Process::Run(const PathName& fileName, const vector<string>& arguments, IRunProcessCallback* callback)
{
  int exitCode;
  MiKTeXException miktexException;
  if (!Run(fileName, arguments, callback, &exitCode, &miktexException, nullptr) || exitCode != 0)
  {
    throw miktexException;
  }
}

bool Process::ExecuteSystemCommand(const string& commandLine)
{
    VALIDATE(false, nullptr);

    return false;
/*
  // {rose-fix}avoid to use process.
  return ExecuteSystemCommand(commandLine, nullptr, nullptr, nullptr);
*/
}

bool Process::ExecuteSystemCommand(const string& commandLine, int* exitCode)
{
    VALIDATE(false, nullptr);

    return false;
/*
  // {rose-fix}avoid to use process.
  return ExecuteSystemCommand(commandLine, exitCode, nullptr, nullptr);
*/
}

vector<string> Process::GetInvokerNames()
{
  vector<string> result;
  result.push_back("rose process");
/*
  // {rose-fix}avoid to use process.
  unique_ptr<Process> process(Process::GetCurrentProcess());
  unique_ptr<Process> parentProcess(process->get_Parent());
  const int maxLevels = 3;
  for (int level = 0; parentProcess.get() != nullptr && level < maxLevels; ++level)
  {
    result.push_back(parentProcess->get_ProcessName());
    parentProcess = parentProcess->get_Parent();
  }
  if (parentProcess.get() != nullptr)
  {
    result.push_back("...");
  }
*/
  reverse(result.begin(), result.end());
  return result;
}
