# C# Saori Proxy for Ukagaka(伺か)

## 在伪春菜中动态编译运行C#代码

用法：

C#中需要一个有public无参构造函数且实现了IDynamicSaori接口的类

伪春菜中调用时第一个参数为选项，后续参数为源码字符串

| 选项         | 说明                                       |
| ------------ | ------------------------------------------ |
| -f<br>--file | 表示后续参数不是源码字符串而是源码文件路径 |

使用[CodeDom](https://docs.microsoft.com/zh-cn/dotnet/api/microsoft.csharp.csharpcodeprovider?view=netframework-4.0)，仅支持C# 5

手动支持#r、#load指令，但需要在前面加上"//"否则无法编译

```C#
//#r "MyLib.dll"
```

```C#
//#load "MyClass.cs"
```

##### 运行环境：.Net Framework 4.0

Debug版本会生成日志文件

#### 已知问题

<font color=red>【致命缺陷】任意方式不退出SSP重载ghost再调用会导致SSP假死，疑似SSP无法重载CLR</font>





## Dynamically compile and run C# code in Ukagaka

Usage: 

C# code needs a class that implements the IDynamicSaori interface and has a public default constructor.

Ukagaka is called with options as the first parameter and source code strings as subsequent parameters.

| Option       | Description                                                  |
| ------------ | ------------------------------------------------------------ |
| -f<br>--file | Indicates that the subsequent parameter is not a source code string but a source code file path |

Using [CodeDom](https://docs.microsoft.com/en-us/dotnet/api/microsoft.csharp.csharpcodeprovider?view=netframework-4.0). Support C# 5 only.

Support #r, #load directive, but need to add "//" in front of it or it won't compile.

```C#
//#r "MyLib.dll"
```

```C#
//#load "MyClass.cs"
```

##### Runtime: .Net Framework 4.0

The Debug version generates log files.

#### Known issues

<font color=red>[Fatal]Any way not to exit SSP reload ghost and then call will lead to SSP false death, it is suspected that SSP can not reload CLR</font>

