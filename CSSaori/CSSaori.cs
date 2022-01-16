using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Resources;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.CSharp;

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
namespace CSSaori
{
	/// <summary>
	/// Standard Saori
	/// </summary>
	public interface ISaori
	{
		bool Load(string saoriDir);
		bool Unload();
		Saori.Result Request(Saori.RequestContext context);
	};

	/// <summary>
	/// Dynamic Saori (ISaori without Load and Unload)
	/// </summary>
	public interface IDynamicSaori
	{
		Saori.Result Request(Saori.RequestContext context);
	};

	public static class Saori
	{
		[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = false)]
		public class EnumStringAttribute : Attribute
		{
			public string Value { get; }
			public EnumStringAttribute(string str) => Value = str;
		}

		public enum Status
		{
			OK = 200,
			[EnumString("No Content")]
			NoContent = 204,
			[EnumString("Bad Request")]
			BadRequest = 400,
			[EnumString("Internal Server Error")]
			InternalServerError = 500
		};
		public enum SecurityLevel
		{
			Unset,
			Local,
			External
		};
		public enum Command
		{
			Unknown,
			[EnumString("GET Version")]
			GetVersion,
			[EnumString("EXECUTE")]
			Execute
		};

		[Serializable]
		public class RequestContext : MarshalByRefObject
		{
			public Command Command { get; set; }
			public string Version { get; set; }
			public SecurityLevel SecurityLevel { get; set; }
			public string[] Args { get; set; }
			public Encoding Encoding { get; set; }
			public string Sender { get; set; }

			public string OriginalCommand { get; set; }
			public string OriginalSecurityLevel { get; set; }
			public string OriginalEncoding { get; set; }
		};

		[Serializable]
		public class Result : MarshalByRefObject
		{
			public Status Status { get; set; } = Status.InternalServerError;
			public Encoding Encoding { get; set; } = DefaultEncoding;
			public string[] Values { get; set; }
		};

		public const string SaoriVersion = "SAORI/1.0";
		public static Encoding DefaultEncoding { get; set; } = Encoding.UTF8;

		static IDictionary<Status, string> StatusValues { get; } = (from Status v in Enum.GetValues(typeof(Status))
																	select new
																	{
																		Key = v,
																		Value = typeof(Status).GetField(v.ToString()).GetCustomAttribute<EnumStringAttribute>(false)?.Value ?? v.ToString()
																	}).ToDictionary(kv => kv.Key, kv => kv.Value);
		static IDictionary<string, Command> Commands { get; } = (from Command c in Enum.GetValues(typeof(Command))
																 select new
																 {
																	 Key = typeof(Command).GetField(c.ToString()).GetCustomAttribute<EnumStringAttribute>(false)?.Value ?? c.ToString(),
																	 Value = c
																 }).ToDictionary(kv => kv.Key, kv => kv.Value);

		public static readonly Regex EncodingRegex = new Regex("(?<=^|\\r\\n)Charset: (?<Encoding>.*?)\\r\\n", RegexOptions.Compiled);
		public static readonly Regex RequestRegex = new Regex("(?<=^|\\r\\n)(?<Command>[^\\r\\n]*?) (?<Version>[^\\s]*?/\\d+\\.\\d+)\\r\\n", RegexOptions.Compiled);
		public static readonly Regex SecurityLevelRegex = new Regex("(?<=^|\\r\\n)SecurityLevel: (?<SecurityLevel>.*?)\\r\\n", RegexOptions.Compiled);
		public static readonly Regex ArgumentsRegex = new Regex("(?<=^|\\r\\n)Argument\\d+: (?<Argument>.*?)(?=\\r\\n)", RegexOptions.Compiled);
		public static readonly Regex SenderRegex = new Regex("(?<=^|\\r\\n)Sender: (?<Sender>.*?)\\r\\n", RegexOptions.Compiled);

		public const string ResultTemplate = "{version} {code} {msg}\r\n{result}{values}Charset: {encoding}\r\n\r\n";
		public const string ResultValueTemplate = "Result: {result}\r\n";
		public const string ValueTemplate = "Value{index}: {value}\r\n";

		public static Encoding GetEncoding(string data)
		{
			var match = EncodingRegex.Match(data);
			if (match.Success)
			{
				try
				{
					return Encoding.GetEncoding(match.Groups[nameof(Encoding)].Value);
				}
				catch (Exception) { }
			}
			return null;
		}

		public static string DecodeBase64Request(string request, Encoding encoding = null)
		{
			var bytes = Convert.FromBase64String(request);
			if (encoding == null)
				encoding = GetEncoding(DefaultEncoding.GetString(bytes));
			return encoding.GetString(bytes);
		}

        public static string EncodeBase64Result(Result result)
        {
			if (result.Encoding == null)
				result.Encoding = DefaultEncoding;
            return Convert.ToBase64String(result.Encoding.GetBytes(SerializeResult(result)));
        }

        public static RequestContext DeserializeRequest(string request)
		{
			var match = RequestRegex.Match(request);
			if (!match.Success)
				return null;
			string oriCommand = match.Groups["Command"].Value;
			var command = Commands.ContainsKey(oriCommand) ? Commands[oriCommand] : Command.Unknown;
			var version = match.Groups["Version"].Value;
			match = SecurityLevelRegex.Match(request);
			string oriSecurityLevel = match.Success ? match.Groups["SecurityLevel"].Value : null;
			if (!match.Success || !Enum.TryParse(oriSecurityLevel, out SecurityLevel securityLevel))
				securityLevel = SecurityLevel.Unset;
			var matches = ArgumentsRegex.Matches(request);
			var argCount = matches.Count;
			var args = new string[argCount];
			for (var i = 0; i < argCount; i++)
				args[i] = matches[i].Groups["Argument"].Value;
			match = EncodingRegex.Match(request);
			Encoding encoding = DefaultEncoding;
			string oriEncoding = "";
			if (match.Success)
			{
				oriEncoding = match.Groups["Encoding"].Value;
				try
				{
					encoding = Encoding.GetEncoding(oriEncoding);
				}
				catch (Exception) { }
			}
			match = SenderRegex.Match(request);
			var sender = match.Success ? match.Groups["Sender"].Value : null;

			return new RequestContext
			{
				Command = command,
				Version = version,
				SecurityLevel = securityLevel,
				Args = args,
				Encoding = encoding,
				Sender = sender,
				OriginalCommand = oriCommand,
				OriginalSecurityLevel = oriSecurityLevel,
				OriginalEncoding = oriEncoding
			};
		}
		public static string SerializeResult(Result result)
		{
			var vals = result.Values ?? new string[] { "" };
			var encoding = result.Encoding ?? DefaultEncoding;
			var status = result.Status;
			var sb = new StringBuilder(ResultTemplate);
			sb.Replace("{version}", "SAORI/1.0");
			sb.Replace("{code}", ((int)status).ToString());
			StatusValues.TryGetValue(status, out var msg);
			sb.Replace("{msg}", msg);
			var values = new StringBuilder("");
			if (vals.Length > 0)
			{
				for (var i = 0; i < vals.Length; i++)
				{
					if (i == 0)
						sb.Replace("{result}", ResultValueTemplate.Replace("{result}", vals[i].Replace("\r\n", "\t")));
					values.Append(ValueTemplate.Replace("{index}", i.ToString()).Replace("{value}", vals[i].Replace("\r\n", "\t")));
				}
			}
			else sb.Replace("{result}", "");
			sb.Replace("{values}", values.Length > 0 ? values.ToString() : "");
			sb.Replace("{encoding}", encoding.WebName.ToUpper().Replace("SHIFT_JIS", "Shift_JIS"));
			return sb.ToString();
		}

		public static Result Error(string msg, Encoding encoding = null) { return Error(new string[] { msg }, encoding); }
		public static Result Error(string[] msg, Encoding encoding = null)
		=> new Result
		{
			Status = Status.InternalServerError,
			Encoding = encoding,
			Values = msg
		};
	}

	public static class Utils
	{
		public static object GetCustomAttribute(this MemberInfo m, Type type, bool inherit = true)
		{
			var attrs = m.GetCustomAttributes(type, inherit);
			return attrs.Length > 0 ? attrs[0] : null;
		}
		public static T GetCustomAttribute<T>(this MemberInfo m, bool inherit = true) where T : Attribute => m.GetCustomAttribute(typeof(T), inherit) as T;
		public static Exception GetInnerException(this Exception err)
        {
			while (err.InnerException != null)
				err = err.InnerException;
			return err;
        }
	}
}
