using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Resources;
using System.Text.RegularExpressions;
using Microsoft.CSharp;

namespace CSSaori
{
    using static CSSaori.Saori;

    class Program
    {
		static void DebugLog(string msg)
        {
#if DEBUG
			File.AppendAllText(typeof(Program).Assembly.Location + ".log", msg);
#endif
		}

		static void Main(string[] args)
        {
			AppDomain.CurrentDomain.AssemblyResolve += (sender, e) =>
			{
				string dllName = e.Name.Contains(",") ? e.Name.Substring(0, e.Name.IndexOf(',')) : e.Name.Replace(".dll", "");
				dllName = dllName.Replace(".", "_");
				if (dllName.EndsWith("_resources")) return null;
				ResourceManager rm = new ResourceManager(nameof(CSSaori) + ".Properties.Resources", Assembly.GetExecutingAssembly());
				byte[] bytes = (byte[])rm.GetObject(dllName);
				return Assembly.Load(bytes);
			};

			Result result;
			try
			{
				if (args.Length < 1)
					throw new ArgumentException("More parameters required");
				DebugLog("\r\nRequest: " + args[0] + Environment.NewLine + DecodeBase64Request(args[0]).Replace("\r\n", "\\r\\n"));
				var context = DeserializeRequest(DecodeBase64Request(args[0]));
				context.Args = context.Args.Skip(1).ToArray();
				if (context == null)
					throw new ArgumentException("Parsing request failed");
                switch (context.Command)
                {
					case Command.GetVersion:
						result = new Result()
						{
							Status = Status.OK,
							Values = new string[] { }
						};
						break;
					case Command.Execute:
						if (context.Args.Length < 1)
							throw new ArgumentException("Need more parameters");

						var cargs = new CompileRun.Args(context.Args[0]);
						if (cargs.UseSandbox)
						{
							var domain = AppDomain.CreateDomain(
								"CompileRunSandbox",
								null,
								null,
								CompileRun.AssemblyPath,
								false
							);
							result = (domain.CreateInstanceFromAndUnwrap(
								typeof(CompileRun).Assembly.Location,
								"CSSaori.CompileRun",
								true,
								BindingFlags.Default,
								null,
								new object[] { cargs, context },
								null,
								null
							) as CompileRun).Result;
							AppDomain.Unload(domain);
						}
						else result = new CompileRun(cargs, context).Result;
						break;
					default:
						result = new Result()
						{
							Status = Status.BadRequest,
							Values = new string[] { "Unsupported Command" }
						};
						break;
				}
			}
            catch (Exception err)
			{
				while (err.InnerException != null)
					err = err.InnerException;
				result = Error(new string[] { "Request failed", err.Message });
			}
			DebugLog("\r\nResult: " + SerializeResult(result).Replace("\r\n", "\\r\\n"));
			Console.WriteLine(EncodeBase64Result(result));
		}

		[Serializable]
		public class CompileRun : MarshalByRefObject
		{
			[Serializable]
			public class Args : MarshalByRefObject
			{
				public bool IsFile { get; }
				public bool UseSandbox { get; }

				static readonly Regex IsFileRegex = new Regex("(?<=^|\\s)([-/]f|--file)(?=\\s|$)", RegexOptions.Compiled | RegexOptions.IgnoreCase);
				static readonly Regex UseSandboxRegex = new Regex("(?<=^|\\s)([-/]s|--sandbox)(?=\\s|$)", RegexOptions.Compiled | RegexOptions.IgnoreCase);

				public Args() { }
				public Args(string args)
				{
					args = Regex.Unescape(args).Trim();

					IsFile = IsFileRegex.IsMatch(args);
					UseSandbox = UseSandboxRegex.IsMatch(args);
				}
			}

			public static string AssemblyPath = Path.GetDirectoryName(typeof(CompileRun).Assembly.Location);
			static readonly Regex LoadRegex = new Regex("(?<=^|[\\r\\n])\\s*//#load\\s+\"(?<File>[^\"]*?)\"\\s*(//.*?)?(?=$|[\\r\\n])", RegexOptions.Compiled);
			static readonly Regex RequireRegex = new Regex("(?<=^|[\\r\\n])\\s*//#r\\s+\"(?<File>[^\"]*?)\"\\s*(//.*?)?(?=$|[\\r\\n])", RegexOptions.Compiled);

			public Result Result { get; }

			public CompileRun(Args args, RequestContext context)
			{
				var sources = new string[context.Args.Length - 1];
				for (var i = 0; i < sources.Length; i++)
					sources[i] = context.Args[i + 1];

				var csc = new CSharpCodeProvider();
				var ps = new CompilerParameters
				{
					GenerateInMemory = true,
					GenerateExecutable = false
				};
				var refs = new HashSet<string>
					{
						"System.dll",
						"System.Core.dll",
						typeof(Saori).Assembly.Location
					};
				var srcfiles = new HashSet<string>();
				var tmpsrcs = new HashSet<string>();
				var tmpkey = Path.GetRandomFileName().Substring(0, 8);
				for (var i = 0; i < sources.Length; i++)
				{
					if (args.IsFile)
					{
						var file = Path.GetFullPath(sources[i]);
						srcfiles.Add(file);
					}
					else
					{
						while (true)
						{
							var tmp = Path.GetTempPath() + "CSSaori_" + tmpkey + "_" + i + ".cs";
							if (!tmpsrcs.Add(tmp))
								continue;
							File.WriteAllText(tmp, sources[i]);
							srcfiles.Add(tmp);
							break;
						}
					}
				}
				var srcs = new Queue<string>(srcfiles);
				var psrcs = new List<string>(srcs.Count * 2 + 1);
				while (srcs.Count > 0)
				{
					var srcfile = srcs.Dequeue();
					string src;
					using (var reader = new StreamReader(srcfile))
						src = reader.ReadToEnd();
					var cwd = Directory.GetCurrentDirectory();
					Directory.SetCurrentDirectory(Path.GetDirectoryName(srcfile));
					var matches = LoadRegex.Matches(src);
					if (matches.Count > 0)
					{
						//src = LoadRegex.Replace(src, "");
						for (var i = 0; i < matches.Count; i++)
						{
							var file = Path.GetFullPath(matches[i].Groups["File"].Value);
							if (srcfiles.Add(file))
								srcs.Enqueue(file);
						}
					}
					matches = RequireRegex.Matches(src);
					if (matches.Count > 0)
					{
						//src = RequireRegex.Replace(src, "");
						for (var i = 0; i < matches.Count; i++)
						{
							var file = matches[i].Groups["File"].Value;
							refs.Add(File.Exists(file) ? Path.GetFullPath(file) : file);
						}
					}
					psrcs.Add(src);
					Directory.SetCurrentDirectory(cwd);
				}
				ps.ReferencedAssemblies.AddRange(refs.ToArray());
				var compile = csc.CompileAssemblyFromFile(ps, srcfiles.ToArray());
				for (var i = tmpsrcs.GetEnumerator(); i.MoveNext();)
					File.Delete(i.Current);

				var errs = new HashSet<string>();
				var iserr = false;
				foreach (CompilerError cerr in compile.Errors)
				{
					errs.Add(cerr.ToString().Replace("\\", "\\\\"));
					iserr = iserr || !cerr.IsWarning;
				}
				if (iserr)
				{
					Result = Error(errs.ToArray(), context.Encoding);
					return;
				}
				var type = (from t in compile.CompiledAssembly.GetTypes()
							where !t.IsAbstract
							where !t.IsGenericType
							where t.GetConstructor(Type.EmptyTypes) != null
							where typeof(IDynamicSaori).IsAssignableFrom(t)
							select t).FirstOrDefault();
				if (type == null)
				{
					Result = Error(nameof(IDynamicSaori) + " not found", context.Encoding);
					return;
				}
				IDynamicSaori saori = null;
				try
				{
					saori = Activator.CreateInstance(type) as IDynamicSaori;
				}
				catch (Exception err)
				{
					Result = Error("Create instance failed: " + err.GetInnerException().Message);
					return;
				}
				try
				{
					Result = saori.Request(context);
					if (Result.Encoding == null)
						Result.Encoding = DefaultEncoding;
				}
				catch (Exception err)
				{
					Result = Error(err.GetInnerException().Message, context.Encoding);
				}
			}
		}
	}
}
