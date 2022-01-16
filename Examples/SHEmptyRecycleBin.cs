using System;
using System.Runtime.InteropServices;
using CSSaori;
public class MySaori : IDynamicSaori
{
    [DllImport("shell32.dll")]
    static extern int SHEmptyRecycleBin(IntPtr handle, string root, int falgs);
    const int NoConfirm  = 1 << 0;
    const int NoProgress = 1 << 1;
    const int NoSound    = 1 << 2;
    public Saori.Result Request(Saori.RequestContext context)
    {
        SHEmptyRecycleBin(IntPtr.Zero, "", NoConfirm | NoProgress);
        return new Saori.Result() {
            Status = Saori.Status.OK,
            Values = new string[] { "Empty Recycle Bin From: " + AppDomain.CurrentDomain.FriendlyName }
        };
    }
}