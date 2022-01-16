# C# Saori Proxy for Ukagaka(伺か)

## 在伪春菜中动态编译运行C#代码

用法：

C#中需要一个有public无参构造函数且实现了IDynamicSaori接口的类

伪春菜中调用SaoriBase64Proxy.dll，第一个参数为带路径的CompileRunCSharp.exe（代理执行目标），第二个参数为选项，后续参数为源码字符串

| 选项         | 说明                                       |
| ------------ | ------------------------------------------ |
| -f<br>--file | 表示后续参数不是源码字符串而是源码文件路径 |

项目分为两部分：[SaoriBase64Proxy](SaoriBase64Proxy)用于将Saori请求编码为base64字符串并通过命令行传递给[CompileRunCSharp](CompileRunCSharp)，后者编译运行后再将数据编码为base64写到*stdout*，由前者重定向*stdout*读取并解析返回。

使用[CodeDom](https://docs.microsoft.com/zh-cn/dotnet/api/microsoft.csharp.csharpcodeprovider?view=netframework-4.0)，仅支持C# 5

手动支持#r、#load指令，但需要在前面加上"//"否则无法编译

```C#
//#r "MyLib.dll"
```

```C#
//#load "MyClass.cs"
```

##### 运行环境：.Net Framework 4.0

Debug版本[CompileRunCSharp](CompileRunCSharp)会生成日志文件





## Dynamically compile and run C# code in Ukagaka

Usage: 

C# code needs a class that implements the IDynamicSaori interface and has a public default constructor.

Ukagaka calls SaoriBase64Proxy.dll, the first parameter is CompileRunCSharp.exe with path (proxy execution target), the second parameter is an option, the subsequent parameters are source strings

| Option       | Description                                                  |
| ------------ | ------------------------------------------------------------ |
| -f<br>--file | Indicates that the subsequent parameter is not a source code string but a source code file path |

The project is divided into two parts: [SaoriBase64Proxy](SaoriBase64Proxy) is used to encode the Saori request as a base64 string and pass it to [CompileRunCSharp](CompileRunCSharp) via the command line, which compiles and runs it and then encodes the data as base64 to *stdout,* which is redirected to *stdout* by the former to read and parse it back.

Using [CodeDom](https://docs.microsoft.com/en-us/dotnet/api/microsoft.csharp.csharpcodeprovider?view=netframework-4.0). Support C# 5 only.

Support #r, #load directive, but need to add "//" in front of it or it won't compile.

```C#
//#r "MyLib.dll"
```

```C#
//#load "MyClass.cs"
```

##### Runtime: .Net Framework 4.0

The [CompileRunCSharp](CompileRunCSharp)'s Debug version generates log files.
