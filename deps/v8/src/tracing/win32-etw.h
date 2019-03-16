// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_WIN32_ETW_H_
#define V8_TRACING_WIN32_ETW_H_

#ifdef V8_OS_WIN

#include "include/v8.h"
#include "src/base/win32-headers.h"

namespace v8 {
namespace etw {

  // See Chakra manifest at https://github.com/Microsoft/ChakraCore/blob/master/manifests/Microsoft-Scripting-Chakra-Instrumentation.man
  // JScript9 GUID: {57277741-3638-4A4B-BDBA-0AC6E45DA56C}
  const GUID providerGuid = { 0x57277741, 0x3638, 0x4A4B, { 0xBD, 0xBA, 0x0a, 0xc6, 0xe4, 0x5d, 0xa5, 0x6c } };
  
  const UCHAR LEVEL_INFO = 4;
  
  const USHORT JSCRIPT_METHOD_LOAD = 9;
  const USHORT JSCRIPT_METHOD_UNLOAD = 10;
  const USHORT JSCRIPT_SOURCE_LOAD = 41;
  const USHORT JSCRIPT_SOURCE_UNLOAD = 42;
  
  const UCHAR METHOD_LOAD_OPCODE = 10;
  const UCHAR METHOD_UNLOAD_OPCODE = 11;
  const UCHAR SOURCE_LOAD_OPCODE = 12;
  const UCHAR SOURCE_UNLOAD_OPCODE = 13;
  
  const USHORT METHOD_RUNTIME_TASK = 1;
  const USHORT SCRIPT_CONTEXT_RUNTIME_TASK = 2;
  
  const ULONGLONG JSCRIPT_RUNTIME_KEYWORD = 1;
  
  const EVENT_DESCRIPTOR MethodLoad = { JSCRIPT_METHOD_LOAD, 0x0, 0x0, LEVEL_INFO, METHOD_LOAD_OPCODE, METHOD_RUNTIME_TASK, JSCRIPT_RUNTIME_KEYWORD };
  const EVENT_DESCRIPTOR MethodUnload = { JSCRIPT_METHOD_UNLOAD, 0x0, 0x0, LEVEL_INFO, METHOD_UNLOAD_OPCODE, METHOD_RUNTIME_TASK, JSCRIPT_RUNTIME_KEYWORD };
  const EVENT_DESCRIPTOR SourceLoad = { JSCRIPT_SOURCE_LOAD, 0x0, 0x0, LEVEL_INFO, SOURCE_LOAD_OPCODE, SCRIPT_CONTEXT_RUNTIME_TASK, JSCRIPT_RUNTIME_KEYWORD };
  const EVENT_DESCRIPTOR SourceUnload = { JSCRIPT_SOURCE_UNLOAD, 0x0, 0x0, LEVEL_INFO, SOURCE_UNLOAD_OPCODE, SCRIPT_CONTEXT_RUNTIME_TASK, JSCRIPT_RUNTIME_KEYWORD };

  REGHANDLE providerHandle = NULL;

  void EtwRegister() {
    DWORD status = EventRegister(&providerGuid, NULL, NULL, &providerHandle);
    if (status != ERROR_SUCCESS) {
        // TODO: Turn into an assert
        OutputDebugStringA("Failed to register with ETW");
    }
  }

  // TODO: EventUnregister on process shutdown

	/* From the event manifest
				<template tid="MethodLoadUnload">
				  <data inType="win:Pointer" name="ScriptContextID"/>
				  <data inType="win:Pointer" name="MethodStartAddress" />
				  <data inType="win:UInt64" name="MethodSize" />
				  <data inType="win:UInt32" name="MethodID" />
				  <data inType="win:UInt16" name="MethodFlags" />
				  <data inType="win:UInt16" map="MethodAddressRangeMap" name="MethodAddressRangeID" />
				  <data inType="win:UInt64" name="SourceID" />
				  <data inType="win:UInt32" name="Line" />
				  <data inType="win:UInt32" name="Column" />
				  <data inType="win:UnicodeString" name="MethodName" />
				</template>
				<template tid="SourceLoadUnload">
				  <data inType="win:UInt64" name="SourceID" />
				  <data inType="win:Pointer" name="ScriptContextID" />
				  <data inType="win:UInt32" name="SourceFlags" />
				  <data inType="win:UnicodeString" name="Url"/>
				</template>	
	*/

  void WriteSourceLoadEvent() {
    if (EventEnabled(providerHandle, &SourceLoad)) {
		  EVENT_DATA_DESCRIPTOR EventData[4];

      UINT64 SourceID = 0;
      PVOID ScriptContextID = nullptr;
      UINT32 SourceFlags = 0;
      WCHAR Url[] = L"https://ticehurst.com/scripts/app.js";

      EventDataDescCreate(&EventData[0], &SourceID, sizeof(const unsigned __int64)  );
      EventDataDescCreate(&EventData[1], &ScriptContextID, sizeof(const void*)  );
      EventDataDescCreate(&EventData[2], &SourceFlags, sizeof(const unsigned int)  );
      EventDataDescCreate(&EventData[3], Url, (ULONG)((wcslen(Url) + 1) * sizeof(WCHAR)));
    }
  }

  void WriteMethodLoadEvent(PVOID script_context, INT64 script_id, PVOID start_address, INT64 length, const char* name, size_t name_len) {
    if (EventEnabled(providerHandle, &MethodLoad)) {
		  EVENT_DATA_DESCRIPTOR EventData[10];

		  UINT32 MethodID = 0;
		  UINT16 MethodFlags = 0;
		  UINT16 MethodAddressRangeID = 0;
		  UINT32 Line = 0;
		  UINT32 Column = 0;
		  WCHAR MethodName[512] = L"[unknown]";
      int name_chars = MultiByteToWideChar(CP_UTF8, 0, name, name_len, MethodName, 512);
      MethodName[name_chars] = 0;

		  EventDataDescCreate(&EventData[0], &script_context, sizeof(const void*));
		  EventDataDescCreate(&EventData[1], &start_address, sizeof(const void*));
		  EventDataDescCreate(&EventData[2], &length, sizeof(const unsigned __int64));
		  EventDataDescCreate(&EventData[3], &MethodID, sizeof(const unsigned int));
		  EventDataDescCreate(&EventData[4], &MethodFlags, sizeof(const unsigned short));
		  EventDataDescCreate(&EventData[5], &MethodAddressRangeID, sizeof(const unsigned short));
		  EventDataDescCreate(&EventData[6], &script_id, sizeof(const unsigned __int64));
		  EventDataDescCreate(&EventData[7], &Line, sizeof(const unsigned int));
		  EventDataDescCreate(&EventData[8], &Column, sizeof(const unsigned int));
		  EventDataDescCreate(&EventData[9], MethodName, (ULONG)((wcslen(MethodName) + 1) * sizeof(WCHAR)));

		  DWORD status = EventWrite(providerHandle, &MethodLoad, 10, EventData);
      if (status != ERROR_SUCCESS) {
          // TODO: Turn into an assert
          OutputDebugStringA("Failed to write ETW event");
      }
    }
  }

  void EtwEventHandler(const JitCodeEvent* event) {
    if (providerHandle == NULL) return;
    if (event->code_type != v8::JitCodeEvent::CodeType::JIT_CODE) return;

    if (event->type == v8::JitCodeEvent::EventType::CODE_ADDED) {
      PVOID script_context = (PVOID)event->isolate; // TODO: Can there be more than one context per isolate?
      INT64 script_id = event->script.IsEmpty() ? 0 : event->script->GetId();
      PVOID start_address = event->code_start;
      INT64 length = (INT64)event->code_len;
      const char* name = event->name.str;
      size_t name_len = event->name.len;

      WriteMethodLoadEvent(script_context, script_id, start_address, length, name, name_len);
    }
  }

} // namespace etw
} // namespace v8

#endif // V8_OS_WIN
#endif // V8_TRACING_WIN32_ETW_H_
