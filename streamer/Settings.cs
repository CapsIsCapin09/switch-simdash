using System;
using System.Collections.Generic;
using System.IO;

namespace SimDashStreamer
{
    // Remembers choices in %AppData%\SimDashStreamer\settings.ini.
    internal sealed class Settings
    {
        public string Source = "";
        public int Port = 18083;
        public int Fps = 30;
        public int Quality = 70;
        public int MaxWidth = 1280;
        public int MaxHeight = 720;
        public bool StartMinimized;

        private static string Dir => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "SimDashStreamer");
        private static string FilePath => Path.Combine(Dir, "settings.ini");

        public static Settings Load()
        {
            var s = new Settings();
            try
            {
                if (!File.Exists(FilePath)) return s;
                var values = new Dictionary<string, string>();
                foreach (var line in File.ReadAllLines(FilePath))
                {
                    int eq = line.IndexOf('=');
                    if (eq > 0) values[line.Substring(0, eq).Trim()] = line.Substring(eq + 1).Trim();
                }
                if (values.TryGetValue("source", out var v)) s.Source = v;
                s.Port = Int(values, "port", s.Port, 1, 65535);
                s.Fps = Int(values, "fps", s.Fps, 5, 60);
                s.Quality = Int(values, "quality", s.Quality, 20, 95);
                s.MaxWidth = Int(values, "max_width", s.MaxWidth, 320, 1920);
                s.MaxHeight = Int(values, "max_height", s.MaxHeight, 180, 1080);
                s.StartMinimized = values.TryGetValue("start_minimized", out v) && v == "1";
            }
            catch (Exception) { }
            return s;
        }

        private static int Int(Dictionary<string, string> values, string key, int fallback, int min, int max)
        {
            if (values.TryGetValue(key, out var v) && int.TryParse(v, out int n) && n >= min && n <= max) return n;
            return fallback;
        }

        public void Save()
        {
            try
            {
                Directory.CreateDirectory(Dir);
                File.WriteAllLines(FilePath, new[]
                {
                    "source=" + Source,
                    "port=" + Port,
                    "fps=" + Fps,
                    "quality=" + Quality,
                    "max_width=" + MaxWidth,
                    "max_height=" + MaxHeight,
                    "start_minimized=" + (StartMinimized ? "1" : "0"),
                });
            }
            catch (Exception) { }
        }
    }
}
