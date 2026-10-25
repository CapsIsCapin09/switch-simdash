using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Text;
using System.Windows.Forms;

namespace SimDashStreamer
{
    // Something that can be captured: a window (even when covered by other
    // windows) or a whole monitor.
    internal sealed class CaptureSource
    {
        public bool IsWindow { get; private set; }
        public string Title { get; private set; }
        public int ScreenIndex { get; private set; }
        private IntPtr hwnd;

        public string Key => IsWindow ? "window:" + Title : "monitor:" + ScreenIndex;

        public override string ToString()
        {
            if (IsWindow) return "Window: " + Title;
            var s = Screen.AllScreens[ScreenIndex];
            return $"Monitor {ScreenIndex + 1}: {s.Bounds.Width}x{s.Bounds.Height}" + (s.Primary ? " (main)" : "");
        }

        public static List<CaptureSource> List()
        {
            var list = new List<CaptureSource>();
            int ownPid = Process.GetCurrentProcess().Id;
            NativeMethods.EnumWindows((h, _) =>
            {
                if (!NativeMethods.IsWindowVisible(h)) return true;
                if (NativeMethods.DwmGetWindowAttribute(h, NativeMethods.DWMWA_CLOAKED, out int cloaked, 4) == 0 && cloaked != 0) return true;
                string title = WindowTitle(h);
                if (string.IsNullOrWhiteSpace(title)) return true;
                NativeMethods.GetWindowThreadProcessId(h, out uint pid);
                if (pid == ownPid) return true;
                list.Add(new CaptureSource { IsWindow = true, Title = title, hwnd = h });
                return true;
            }, IntPtr.Zero);

            // Likely SimHub dash windows first.
            list.Sort((a, b) => Score(b).CompareTo(Score(a)));
            for (int i = 0; i < Screen.AllScreens.Length; i++)
                list.Add(new CaptureSource { IsWindow = false, ScreenIndex = i });
            return list;
        }

        private static int Score(CaptureSource s)
        {
            string t = s.Title.ToLowerInvariant();
            int score = 0;
            if (t.Contains("simhub")) score += 2;
            if (t.Contains("dash")) score += 1;
            return score;
        }

        public static CaptureSource FromKey(string key, List<CaptureSource> available)
        {
            foreach (var s in available)
                if (s.Key == key) return s;
            return null;
        }

        private static string WindowTitle(IntPtr h)
        {
            int len = NativeMethods.GetWindowTextLength(h);
            if (len <= 0) return null;
            var sb = new StringBuilder(len + 1);
            NativeMethods.GetWindowText(h, sb, sb.Capacity);
            return sb.ToString();
        }

        // Finds the window again if it was closed and reopened.
        private bool ResolveWindow()
        {
            if (hwnd != IntPtr.Zero && NativeMethods.IsWindow(hwnd) && WindowTitle(hwnd) == Title) return true;
            hwnd = IntPtr.Zero;
            NativeMethods.EnumWindows((h, _) =>
            {
                if (NativeMethods.IsWindowVisible(h) && WindowTitle(h) == Title)
                {
                    hwnd = h;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return hwnd != IntPtr.Zero;
        }

        private Bitmap raw;

        // Returns the captured picture, or null if the source is unavailable.
        // The bitmap is reused between calls.
        public Bitmap Capture(out string problem)
        {
            problem = null;
            int w, h;
            if (IsWindow)
            {
                if (!ResolveWindow()) { problem = "Window not found: " + Title; return null; }
                if (NativeMethods.IsIconic(hwnd)) { problem = "The dash window is minimized. Restore it (it can sit behind other windows)."; return null; }
                NativeMethods.GetClientRect(hwnd, out var rc);
                w = rc.Right - rc.Left;
                h = rc.Bottom - rc.Top;
            }
            else
            {
                if (ScreenIndex >= Screen.AllScreens.Length) { problem = "Monitor not found"; return null; }
                var b = Screen.AllScreens[ScreenIndex].Bounds;
                w = b.Width;
                h = b.Height;
            }
            if (w <= 0 || h <= 0) { problem = "Nothing to capture"; return null; }

            if (raw == null || raw.Width != w || raw.Height != h)
            {
                raw?.Dispose();
                raw = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppRgb);
            }
            using (var g = Graphics.FromImage(raw))
            {
                if (IsWindow)
                {
                    IntPtr hdc = g.GetHdc();
                    bool ok = NativeMethods.PrintWindow(hwnd, hdc, NativeMethods.PW_CLIENTONLY | NativeMethods.PW_RENDERFULLCONTENT);
                    g.ReleaseHdc(hdc);
                    if (!ok) { problem = "Couldn't capture the window"; return null; }
                }
                else
                {
                    var b = Screen.AllScreens[ScreenIndex].Bounds;
                    g.CopyFromScreen(b.Location, Point.Empty, b.Size, CopyPixelOperation.SourceCopy);
                }
            }
            return raw;
        }
    }

    internal static class Scaler
    {
        private static Bitmap scaled;

        // Shrinks the picture to fit maxW x maxH, keeping its shape.
        public static Bitmap Fit(Bitmap src, int maxW, int maxH)
        {
            double scale = Math.Min(1.0, Math.Min((double)maxW / src.Width, (double)maxH / src.Height));
            if (scale >= 1.0) return src;
            int w = Math.Max(1, (int)(src.Width * scale)), h = Math.Max(1, (int)(src.Height * scale));
            if (scaled == null || scaled.Width != w || scaled.Height != h)
            {
                scaled?.Dispose();
                scaled = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format24bppRgb);
            }
            using (var g = Graphics.FromImage(scaled))
            {
                g.InterpolationMode = InterpolationMode.Bilinear;
                g.PixelOffsetMode = PixelOffsetMode.HighSpeed;
                g.CompositingQuality = CompositingQuality.HighSpeed;
                g.DrawImage(src, 0, 0, w, h);
            }
            return scaled;
        }
    }
}
