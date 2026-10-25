using System;
using System.Drawing;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Windows.Forms;

namespace SimDashStreamer
{
    internal sealed class MainForm : Form
    {
        private readonly Settings settings = Settings.Load();
        private readonly StreamServer server;
        private readonly ComboBox sourceBox = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 380 };
        private readonly NumericUpDown fpsBox = new NumericUpDown { Minimum = 5, Maximum = 60, Width = 70 };
        private readonly NumericUpDown qualityBox = new NumericUpDown { Minimum = 20, Maximum = 95, Width = 70 };
        private readonly CheckBox minimizedBox = new CheckBox { Text = "Start minimized to the tray", AutoSize = true };
        private readonly Label status = new Label { AutoSize = false, Width = 470, Height = 40 };
        private readonly NotifyIcon tray = new NotifyIcon();

        public MainForm()
        {
            Text = "SimDash Streamer";
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            ClientSize = new Size(500, 300);
            Font = new Font("Segoe UI", 9.5f);
            Icon = SystemIcons.Application;

            var layout = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, Padding = new Padding(12), WrapContents = false };
            Controls.Add(layout);

            layout.Controls.Add(new Label { Text = "Send this to the Switch (open your SimHub dash in a window first):", AutoSize = true });
            var sourceRow = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
            var refresh = new Button { Text = "Refresh", Width = 80 };
            refresh.Click += (s, e) => LoadSources();
            sourceRow.Controls.Add(sourceBox);
            sourceRow.Controls.Add(refresh);
            layout.Controls.Add(sourceRow);

            var tuneRow = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 8, 0, 0) };
            tuneRow.Controls.Add(new Label { Text = "Frames per second", AutoSize = true, Margin = new Padding(0, 6, 4, 0) });
            tuneRow.Controls.Add(fpsBox);
            tuneRow.Controls.Add(new Label { Text = "Picture quality", AutoSize = true, Margin = new Padding(16, 6, 4, 0) });
            tuneRow.Controls.Add(qualityBox);
            layout.Controls.Add(tuneRow);
            layout.Controls.Add(minimizedBox);

            layout.Controls.Add(new Label { Text = "On the Switch, set the PC address in SimDash to:", AutoSize = true, Margin = new Padding(0, 12, 0, 0) });
            layout.Controls.Add(new Label { Text = LocalAddresses(), AutoSize = true, Font = new Font(Font, FontStyle.Bold) });
            layout.Controls.Add(status);

            fpsBox.Value = settings.Fps;
            qualityBox.Value = settings.Quality;
            minimizedBox.Checked = settings.StartMinimized;

            server = new StreamServer(settings.Port)
            {
                Fps = settings.Fps,
                Quality = settings.Quality,
                MaxWidth = settings.MaxWidth,
                MaxHeight = settings.MaxHeight,
            };
            server.StatusChanged += text =>
            {
                if (IsHandleCreated && !IsDisposed)
                    BeginInvoke((Action)(() => { status.Text = text; tray.Text = Trim63(text); }));
            };

            sourceBox.SelectedIndexChanged += (s, e) =>
            {
                var src = sourceBox.SelectedItem as CaptureSource;
                server.Source = src;
                if (src != null) settings.Source = src.Key;
                settings.Save();
            };
            fpsBox.ValueChanged += (s, e) => { server.Fps = settings.Fps = (int)fpsBox.Value; settings.Save(); };
            qualityBox.ValueChanged += (s, e) => { server.Quality = settings.Quality = (int)qualityBox.Value; settings.Save(); };
            minimizedBox.CheckedChanged += (s, e) => { settings.StartMinimized = minimizedBox.Checked; settings.Save(); };

            tray.Icon = SystemIcons.Application;
            tray.Text = "SimDash Streamer";
            tray.Visible = true;
            tray.DoubleClick += (s, e) => ShowFromTray();
            tray.ContextMenu = new ContextMenu(new[]
            {
                new MenuItem("Show", (s, e) => ShowFromTray()),
                new MenuItem("Exit", (s, e) => Close()),
            });

            LoadSources();
        }

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            try
            {
                server.Start();
            }
            catch (SocketException ex)
            {
                status.Text = $"Can't listen on port {settings.Port}: {ex.Message}";
            }
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (settings.StartMinimized) Hide();
        }

        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            if (WindowState == FormWindowState.Minimized) Hide();
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            tray.Visible = false;
            server.Stop();
            base.OnFormClosed(e);
        }

        private void ShowFromTray()
        {
            Show();
            WindowState = FormWindowState.Normal;
            Activate();
        }

        private void LoadSources()
        {
            var sources = CaptureSource.List();
            sourceBox.BeginUpdate();
            sourceBox.Items.Clear();
            foreach (var s in sources) sourceBox.Items.Add(s);
            sourceBox.EndUpdate();
            var saved = CaptureSource.FromKey(settings.Source, sources);
            if (saved != null) sourceBox.SelectedItem = saved;
            else if (sourceBox.Items.Count > 0) sourceBox.SelectedIndex = 0;
        }

        private static string LocalAddresses()
        {
            var addrs = NetworkInterface.GetAllNetworkInterfaces()
                .Where(n => n.OperationalStatus == OperationalStatus.Up && n.NetworkInterfaceType != NetworkInterfaceType.Loopback)
                .SelectMany(n => n.GetIPProperties().UnicastAddresses)
                .Select(a => a.Address)
                .Where(a => a.AddressFamily == AddressFamily.InterNetwork && !IPAddress.IsLoopback(a) && !a.ToString().StartsWith("169.254."))
                .Select(a => a.ToString())
                .Distinct()
                .ToArray();
            return addrs.Length > 0 ? string.Join("   or   ", addrs) : "(no network found)";
        }

        private static string Trim63(string s) => s.Length <= 63 ? s : s.Substring(0, 63);
    }
}
