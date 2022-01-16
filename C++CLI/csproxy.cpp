#include "pch.h"

#define CSPROXY_DLLEXPORT
//#define CSPROXY_DYNAMIC_SAORI
#include "csproxy.h"
#include <string>

#pragma managed

#ifdef GetTempPath
#undef GetTempPath
#endif
#using <mscorlib.dll>
#using <System.dll>
#using <System.Core.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::IO;
using namespace System::Text;
using namespace System::Text::RegularExpressions;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;
using namespace System::CodeDom::Compiler;
using namespace Microsoft::CSharp;

using namespace System;
using namespace System::Reflection;
using namespace System::Runtime::CompilerServices;
using namespace System::Runtime::InteropServices;
using namespace System::Security::Permissions;

[assembly:AssemblyTitleAttribute(L"CSSaoriProxy")] ;
[assembly:AssemblyDescriptionAttribute(L"C# Saori Proxy for Ukagaka")] ;
[assembly:AssemblyConfigurationAttribute(L"")] ;
[assembly:AssemblyCompanyAttribute(L"")] ;
[assembly:AssemblyProductAttribute(L"C# Saori Proxy for Ukagaka")] ;
[assembly:AssemblyCopyrightAttribute(L"Copyleft")] ;
[assembly:AssemblyTrademarkAttribute(L"")] ;
[assembly:AssemblyCultureAttribute(L"")] ;

[assembly:AssemblyVersionAttribute("1.0.0.0")] ;

[assembly:ComVisible(true)] ;

namespace CSSaori {

#define var auto

	public ref class Saori
	{
	public:
		const char* SaoriVersion = "SAORI/1.0";
		static Encoding^ DefaultEncoding = Encoding::UTF8;

		// 下划线将在使用时替换为空格
		enum class Status {
			Unknown = 0,
			OK = 200,
			No_Content = 204,
			Bad_Request = 400,
			Internal_Server_Error = 500
		};
		enum class SecurityLevel {
			Unset = 0,
			Local,
			External
		};
		enum class Command {
			Invalid = 0,
			GET_Version,
			EXECUTE
		};

		[Serializable]
		ref struct RequestContext : MarshalByRefObject {
			Command Command;
			String^ Version;
			SecurityLevel SecurityLevel;
			array<String^>^ Args;
			Encoding^ Encoding;
			String^ Sender;

			String^ OriginalCommand;
			String^ OriginalSecurityLevel;
			String^ OriginalEncoding;
		};

		[Serializable]
		ref struct Result : MarshalByRefObject {
			Status Status = Status::Internal_Server_Error;
			Encoding^ Encoding = Saori::DefaultEncoding;
			array<String^>^ Values = nullptr;
		};

		static IDictionary<int, String^>^ StateMessages = gcnew Dictionary<int, String^>(16);

#ifndef CSPROXY_DYNAMIC_SAORI
		static String^ SaoriDir;
#endif
		static Regex^ EncodingRegex = gcnew Regex("(?<=^|\\r\\n)Charset: (?<Encoding>.*?)\\r\\n", RegexOptions::Compiled);
		static Regex^ RequestRegex = gcnew Regex("(?<=^|\\r\\n)(?<Command>[^\\r\\n]*?) (?<Version>[^\\s]*?/\\d+\\.\\d+)\\r\\n", RegexOptions::Compiled);
		static Regex^ SecurityLevelRegex = gcnew Regex("(?<=^|\\r\\n)SecurityLevel: (?<SecurityLevel>.*?)\\r\\n", RegexOptions::Compiled);
		static Regex^ ArgumentsRegex = gcnew Regex("(?<=^|\\r\\n)Argument\\d+: (?<Argument>.*?)(?=\\r\\n)", RegexOptions::Compiled);
		static Regex^ SenderRegex = gcnew Regex("(?<=^|\\r\\n)Sender: (?<Sender>.*?)\\r\\n", RegexOptions::Compiled);

		static String^ ResultTemplate = "{version} {code} {msg}\r\n{result}{values}Charset: {encoding}\r\n\r\n";
		static String^ ResultValueTemplate = "Result: {result}\r\n";
		static String^ ValueTemplate = "Value{index}: {value}\r\n";

		/* args:
	(Command Version)[CRLF] (例: EXECUTE SAORI/1.0)
	SecurityLevel: (Local|External)[CRLF]
	Argument0: Value[CRLF] (例: Argument0: GetMD5)
	Argument1: Value[CRLF]
	Argument2: Value[CRLF]
		・
		・
		・
	Argument[n]: Value[CRLF]
	Charset: (Shift_JIS|ISO-2022-JP|EUC-JP|UTF-8)[CRLF]
	Sender: (ShioriName)[CRLF]
	[CRLF]
		*/
		/* result:
	version statuscode statusstring[CRLF] (例: SAORI/1.0 200 OK)
	Result: Scalar-Value[CRLF]
	Value0: Vector-Value[CRLF]
	Value1: Vector-Value[CRLF]
	Value2: Vector-Value[CRLF]
		・
		・
		・
	Value[n]: Vector-Value[CRLF]
	Charset: (Shift_JIS|ISO-2022-JP|EUC-JP|UTF-8)[CRLF]
	[CRLF]
		*/

		static Saori() {
			var statusList = dynamic_cast<array<Status>^>(Enum::GetValues(Status::typeid));
			for (var i = 0; i < statusList->Length; i++)
				StateMessages->Add((int)statusList[i], statusList[i].ToString()->Replace('_', ' '));
		}

		static Encoding^ GetEncoding(String^ data) {
			var match = EncodingRegex->Match(data);
			if (match->Success) {
				try {
					return Encoding::GetEncoding(match->Groups[nameof(Encoding)]->Value);
				}
				catch (Exception^) {}
			}
			return nullptr;
		}

		static String^ DecodeString(HGLOBAL h, long len) { return DecodeString(h, len, DefaultEncoding); }
		static String^ DecodeString(HGLOBAL h, long len, Encoding^ encoding) {
			var str = gcnew String((char*)h, 0, len, encoding);
			encoding = GetEncoding(str);
			if (encoding != nullptr)
				str = gcnew String((char*)h, 0, len, encoding);
			GlobalFree(h);
			return str;
		}
		static HGLOBAL EncodeString(String^ str, long* len) { return EncodeString(str, len, DefaultEncoding); }
		static HGLOBAL EncodeString(String^ str, long* len, Encoding^ encoding) {
			var bytes = encoding->GetBytes(str);
			*len = bytes->Length;
			var h = GlobalAlloc(GMEM_FIXED, *len);
			Marshal::Copy(bytes, 0, IntPtr(h), *len);
			return h;
		}

		static RequestContext^ DeserializeRequest(String^ data) {
			var match = RequestRegex->Match(data);
			if (!match->Success)
				return nullptr;
			Command command;
			String^ oriCommand = match->Groups["Command"]->Value;
			if (!Enum::TryParse<Command>(oriCommand->Replace(' ', '_'), command))
				return nullptr;
			var version = match->Groups["Version"]->Value;
			match = SecurityLevelRegex->Match(data);
			SecurityLevel securityLevel;
			String^ oriSecurityLevel = match->Success ? match->Groups["SecurityLevel"]->Value : nullptr;
			if (!match->Success || !Enum::TryParse<SecurityLevel>(oriSecurityLevel, securityLevel))
				securityLevel = SecurityLevel::Unset;
			var matches = ArgumentsRegex->Matches(data);
			var argCount = matches->Count;
			var args = gcnew array<String^>(argCount);
			for (var i = 0; i < argCount; i++)
				args[i] = matches[i]->Groups["Argument"]->Value;
			match = EncodingRegex->Match(data);
			Encoding^ encoding;
			String^ oriEncoding = "";
			if (match->Success) {
				oriEncoding = match->Groups["Encoding"]->Value;
				try {
					encoding = Encoding::GetEncoding(oriEncoding);
				}
				catch (Exception^) { encoding = DefaultEncoding; }
			}
			match = SenderRegex->Match(data);
			var sender = match->Success ? match->Groups["Sender"]->Value : nullptr;

			var context = gcnew RequestContext();
			context->Command = command;
			context->Version = version;
			context->SecurityLevel = securityLevel;
			context->Args = args;
			context->Encoding = encoding;
			context->Sender = sender;
			context->OriginalCommand = oriCommand;
			context->OriginalSecurityLevel = oriSecurityLevel;
			context->OriginalEncoding = oriEncoding;

			return context;
		}
		static String^ SerializeResult(Result^ result) {
			var vals = (result->Values != nullptr) ? result->Values : gcnew array<String^> { "" };
			var encoding = result->Encoding != nullptr ? result->Encoding : Saori::DefaultEncoding;
			var status = result->Status != Status::Unknown ? result->Status : Status::Internal_Server_Error;
			var sb = gcnew StringBuilder(ResultTemplate);
			sb->Replace("{version}", "SAORI/1.0");
			sb->Replace("{code}", ((int)status).ToString());
			String^ msg;
			StateMessages->TryGetValue((int)status, msg);
			sb->Replace("{msg}", msg);
			var values = gcnew StringBuilder("");
			if (vals->Length > 0) {
				for (var i = 0; i < vals->Length; i++) {
					if (i == 0)
						sb->Replace("{result}", ResultValueTemplate->Replace("{result}", vals[i]->Replace("\r\n", "\t")));
					values->Append(ValueTemplate->Replace("{index}", i.ToString())->Replace("{value}", vals[i]->Replace("\r\n", "\t")));
				}
			}
			else sb->Replace("{result}", "");
			sb->Replace("{values}", values->Length > 0 ? values->ToString() : "");
			sb->Replace("{encoding}", encoding->WebName->ToUpper()->Replace("SHIFT_JIS", "Shift_JIS"));
			return sb->ToString();
		}

		static Saori::Result^ Error(String^ msg) { return Error(gcnew array<String^>{ msg }, DefaultEncoding); }
		static Saori::Result^ Error(array<String^>^ msg) { return Error(msg, DefaultEncoding); }
		static Saori::Result^ Error(String^ msg, Encoding^ encoding) { return Error(gcnew array<String^>{ msg }, encoding); }
		static Saori::Result^ Error(array<String^>^ msg, Encoding^ encoding) {
			var result = gcnew Saori::Result();
			result->Status = Saori::Status::Internal_Server_Error;
			result->Encoding = encoding;
			result->Values = msg;
			return result;
		}
	};

	[Serializable]
	public ref struct CompileArgs : MarshalByRefObject {
		bool IsFile;

		CompileArgs() {}
		CompileArgs(String^ cargs) {
			cargs = Regex::Unescape(cargs)->Trim();
			var p = Marshal::StringToHGlobalUni(cargs);
			std::wstring args((const TCHAR*)p.ToPointer());
			Marshal::FreeHGlobal(p);
			p = Marshal::StringToHGlobalUni(cargs->ToLower());
			std::wstring largs((const TCHAR*)p.ToPointer());
			Marshal::FreeHGlobal(p);

			var offset = 0;
			while (largs[offset] != TEXT('\0')) {
				switch (largs[offset]) {
				case TEXT('-'):
				case TEXT('/'):
					switch (largs[offset + 1]) {
					case TEXT('f'):
						switch (largs[offset + 2]) {
						case TEXT(' '):
						case TEXT('\0'):
						case TEXT('\r'):
						case TEXT('\n'):
							IsFile = true;
							offset += 2;
							break;
						default:
							offset++;
							break;
						}
						break;
					case TEXT('-'):
						switch (largs[offset + 2]) {
						case TEXT('f'):
							switch (largs[offset + 6]) {
							case ' ':
							case TEXT('\0'):
							case TEXT('\r'):
							case TEXT('\n'):
								if (wmemcmp(&largs[offset + 1], TEXT("file"), 4)) {
									IsFile = true;
									offset += 6;
									break;
								}
								break;
							default:
								offset++;
								break;
							}
							offset++;
							break;
						default:
							offset++;
							break;
						}
						break;
					default:
						offset++;
						break;
					}
					break;
				default:
					while (args[offset + 1] != TEXT('\0')) {
						offset++;
						if (args[offset] == TEXT(' '))
							break;
					}
					offset++;
					break;
				}
			}
		}
	};

	/// <summary>
	/// Standard Saori
	/// </summary>
	public interface class ISaori {
		bool Load(String^ saoriDir);
		bool Unload();
		Saori::Result^ Request(Saori::RequestContext^ context);
	};
	/// <summary>
	/// Dynamic Saori (ISaori without Load and Unload)
	/// </summary>
	public interface class IDynamicSaori {
		Saori::Result^ Request(Saori::RequestContext^ context);
	};

	public ref class CompileRun : MarshalByRefObject {
		static bool IsSaoriClass(Type^ t) { return !t->IsAbstract && !t->IsGenericType && t->GetConstructor(Type::EmptyTypes) != nullptr; }
		static bool _IsSaori(Type^ t) { return IsSaoriClass(t) && ISaori::typeid->IsAssignableFrom(t); }
		static bool _IsDynamicSaori(Type^ t) { return IsSaoriClass(t) && IDynamicSaori::typeid->IsAssignableFrom(t); }
		static Predicate<Type^>^ IsSaori = gcnew Predicate<Type^>(&_IsSaori);
		static Predicate<Type^>^ IsDynamicSaori = gcnew Predicate<Type^>(&_IsDynamicSaori);
	public:
		[Serializable]
		ref struct Result : MarshalByRefObject {
			String^ Result;
		};
		static String^ AssemblyPath = Path::GetDirectoryName(CompileRun::typeid->Assembly->Location);
		static Regex^ LoadRegex = gcnew Regex("(?<=^|[\\r\\n])\\s*//#load\\s+\"(?<File>[^\"]*?)\"\\s*(//.*?)?(?=$|[\\r\\n])", RegexOptions::Compiled);
		static Regex^ RequireRegex = gcnew Regex("(?<=^|[\\r\\n])\\s*//#r\\s+\"(?<File>[^\"]*?)\"\\s*(//.*?)?(?=$|[\\r\\n])", RegexOptions::Compiled);

		static String^ ReadSource(String^ file) {
			var reader = gcnew StreamReader(file);
			var src = reader->ReadToEnd();
			reader->~StreamReader();
			return src;
		}

		CompileRun(Saori::RequestContext^ context, Result^ result) {
			var args = gcnew CompileArgs(context->Args[0]);
			var sources = gcnew array<String^>(context->Args->Length - 1);
			for (var i = 0; i < sources->Length; i++)
				sources[i] = context->Args[i + 1];
			Run(args, sources, context, result);
		}

		static void Run(CompileArgs^ args, array<String^>^ sources, Saori::RequestContext^ context, Result^ result) {
			Saori::Result^ ret;
			do {
				var csc = gcnew CSharpCodeProvider();
				var params = gcnew CompilerParameters();
				params->GenerateInMemory = true;
				params->GenerateExecutable = false;
				var refs = gcnew HashSet<String^>();
				refs->Add("System.dll");
				refs->Add("System.Core.dll");
				refs->Add(Saori::typeid->Assembly->Location);
				var srcfiles = gcnew HashSet<String^>();
				var tmpsrcs = gcnew HashSet<String^>();
				var tmpkey = Path::GetRandomFileName()->Substring(0, 8);
				for (var i = 0; i < sources->Length; i++) {
					if (args->IsFile) {
						var file = Path::GetFullPath(sources[i]);
						srcfiles->Add(file);
					}
					else {
						while (true) {
							var tmp = Path::GetTempPath() + "CSSaori_" + tmpkey + "_" + i + ".cs";
							if (!tmpsrcs->Add(tmp))
								continue;
							File::WriteAllText(tmp, sources[i]);
							srcfiles->Add(tmp);
							break;
						}
					}
				}
				var srcs = gcnew Queue<String^>(srcfiles);
				var psrcs = gcnew List<String^>(srcs->Count * 2 + 1);
				while (srcs->Count > 0) {
					var srcfile = srcs->Dequeue();
					var src = ReadSource(srcfile);
					var cwd = Directory::GetCurrentDirectory();
					Directory::SetCurrentDirectory(Path::GetDirectoryName(srcfile));
					var matches = LoadRegex->Matches(src);
					if (matches->Count > 0) {
						//src = LoadRegex->Replace(src, "");
						for (var i = 0; i < matches->Count; i++) {
							var file = Path::GetFullPath(matches[i]->Groups["File"]->Value);
							if (srcfiles->Add(file))
								srcs->Enqueue(file);
						}
					}
					matches = RequireRegex->Matches(src);
					if (matches->Count > 0) {
						//src = RequireRegex->Replace(src, "");
						for (var i = 0; i < matches->Count; i++) {
							var file = matches[i]->Groups["File"]->Value;
							refs->Add(File::Exists(file) ? Path::GetFullPath(file) : file);
						}
					}
					psrcs->Add(src);
					Directory::SetCurrentDirectory(cwd);
				}
				params->ReferencedAssemblies->AddRange((gcnew List<String^>(refs))->ToArray());
				var compile = csc->CompileAssemblyFromFile(params, (gcnew List<String^>(srcfiles))->ToArray());
				for (var i = tmpsrcs->GetEnumerator(); i.MoveNext();)
					File::Delete(i.Current);

				var errs = gcnew HashSet<String^>();
				var err = false;
				for (var i = compile->Errors->GetEnumerator(); i->MoveNext();) {
					auto cerr = dynamic_cast<CompilerError^>(i->Current);
					errs->Add(cerr->ToString()->Replace("\\", "\\\\"));
					err = err || !cerr->IsWarning;
				}
				if (err) {
					ret = Saori::Error((gcnew List<String^>(errs))->ToArray(), context->Encoding);
					break;
				}
				var types = compile->CompiledAssembly->GetTypes();
				var type = types->Find(types, IsDynamicSaori);
				if (type == nullptr) {
					ret = Saori::Error(nameof(IDynamicSaori) + " not found", context->Encoding);
					break;
				}
				IDynamicSaori^ saori = nullptr;
				try {
					saori = dynamic_cast<IDynamicSaori^>(Activator::CreateInstance(type));
				}
				catch (Exception^ err) {
					while (err->InnerException != nullptr)
						err = err->InnerException;
					ret = Saori::Error("Create instance failed: " + err->Message);
					break;
				}
				try {
					ret = saori->Request(context);
					if (ret->Encoding == nullptr)
						ret->Encoding = Saori::DefaultEncoding;
				}
				catch (Exception^ err) {
					ret = Saori::Error(err->Message, context->Encoding);
				}
			} while (false);
			result->Result = Saori::SerializeResult(ret);
		}
	};
}

using namespace CSSaori;

#if DEBUG || _DEBUG
#ifndef CSPROXY_DYNAMIC_SAORI
#define DebugLogDir Saori::SaoriDir
#else
#define DebugLogDir CompileRun::AssemblyPath
#endif
#define DebugLog(x) File::AppendAllText(DebugLogDir + "/CSSaoriProxy.log", (gcnew String(x))->Replace("\r", "\\r")->Replace("\n", "\\n") + "\r\n")
#else
#define DebugLog(x)
#endif

#ifndef CSPROXY_DYNAMIC_SAORI
static bool Load(HGLOBAL str, long len) {
	Saori::SaoriDir = Saori::DecodeString(str, len); // load函数传的值总是使用Shift_JIS编码？
	DebugLog("Load: \t" + Saori::SaoriDir);
	return true;
}

static bool Unload() {
	DebugLog("Unload");
	return true;
}
#endif

static HGLOBAL Request(HGLOBAL str, long* len) {
	try {
		var req = Saori::DecodeString(str, *len);
		DebugLog("Request: \t" + req);
		var context = Saori::DeserializeRequest(req);
		if (context == nullptr)
			throw gcnew ArgumentException("Parsing request failed");
		HGLOBAL rep;
		if (context->Command == Saori::Command::GET_Version) {
			var result = gcnew Saori::Result();
			result->Status = Saori::Status::OK;
			result->Values = gcnew array<String^>{};
			var str = Saori::SerializeResult(result);
			DebugLog("Result: \t" + str);
			rep = Saori::EncodeString(str, len);
		}
		else if (context->Command == Saori::Command::EXECUTE) {
			if (context->Args->Length < 1)
				throw gcnew ArgumentException("Need more parameters");

			var domain = AppDomain::CreateDomain(
				"CompileRunSandbox",
				nullptr,
				nullptr,
				CompileRun::AssemblyPath,
				false
			);
			var result = gcnew CompileRun::Result();
			domain->CreateInstanceFromAndUnwrap(
				CompileRun::typeid->Assembly->Location,
				"CSSaori.CompileRun",
				true,
				BindingFlags::Default,
				nullptr,
				gcnew array<Object^>{ context, result },
				nullptr,
				nullptr
			);
			var encoding = Saori::GetEncoding(result->Result);
			DebugLog("Result: \t" + result->Result);
			rep = Saori::EncodeString(result->Result, len, encoding);
			domain->Unload(domain);
		}
		else {
			var result = gcnew Saori::Result();
			result->Status = Saori::Status::Bad_Request;
			result->Values = gcnew array<String^>{ "Unsupported Command" };
			var str = Saori::SerializeResult(result);
			DebugLog("Result: \t" + str);
			rep = Saori::EncodeString(str, len);
		}
		return rep;
	}
	catch (Exception^ err) {
		while (err->InnerException != nullptr)
			err = err->InnerException;
		var result = Saori::Error(gcnew array<String^>{ "Request failed", err->Message });
		var str = Saori::SerializeResult(result);
		DebugLog("Result: \t" + str);
		return Saori::EncodeString(str, len, result->Encoding);
	}
}

#pragma unmanaged

#ifndef CSPROXY_DYNAMIC_SAORI
BOOL	CSPROXY_LIBRARY_MODE load	(HGLOBAL h, long len)	{ return Load(h, len); }
BOOL	CSPROXY_LIBRARY_MODE unload	()						{ return Unload(); }
#endif
HGLOBAL	CSPROXY_LIBRARY_MODE request(HGLOBAL h, long* len)	{ return Request(h, len); }
