using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace SimDashStreamer
{
    // Sends JPEG frames to SimDash on the Switch.
    // Wire format: "SDFR" + uint32 little-endian length + JPEG bytes.
    // The Switch answers each frame with one byte ('A'), and the next frame
    // is only sent after that, so frames never pile up and add delay.
    internal sealed class StreamServer
    {
        public volatile CaptureSource Source;
        public volatile int Fps = 30;
        public volatile int Quality = 70;
        public volatile int MaxWidth = 1280;
        public volatile int MaxHeight = 720;

        public event Action<string> StatusChanged;

        private readonly int port;
        private Thread thread;
        private TcpListener listener;
        private volatile bool running;

        public StreamServer(int port)
        {
            this.port = port;
        }

        public void Start()
        {
            running = true;
            listener = new TcpListener(IPAddress.Any, port);
            listener.Start();
            thread = new Thread(Run) { IsBackground = true, Name = "stream" };
            thread.Start();
        }

        public void Stop()
        {
            running = false;
            try { listener?.Stop(); } catch { }
            thread?.Join(2000);
        }

        private void Status(string s) => StatusChanged?.Invoke(s);

        private void Run()
        {
            while (running)
            {
                Status($"Waiting for the Switch on port {port}");
                TcpClient client;
                try { client = listener.AcceptTcpClient(); }
                catch { return; }
                using (client)
                {
                    string who = ((IPEndPoint)client.Client.RemoteEndPoint).Address.ToString();
                    try { Serve(client, who); }
                    catch (Exception) { }
                }
            }
        }

        private void Serve(TcpClient client, string who)
        {
            client.NoDelay = true;
            client.SendBufferSize = 1024 * 1024;
            client.ReceiveTimeout = 5000;
            var stream = client.GetStream();
            var jpeg = new MemoryStream(512 * 1024);
            var encoder = ImageCodecInfo.GetImageEncoders().First(c => c.FormatID == ImageFormat.Jpeg.Guid);
            var header = new byte[8] { (byte)'S', (byte)'D', (byte)'F', (byte)'R', 0, 0, 0, 0 };
            var ack = new byte[1];
            var clock = Stopwatch.StartNew();
            var fpsClock = Stopwatch.StartNew();
            int frames = 0;
            string lastProblem = null;
            Status($"Streaming to Switch at {who}");

            while (running)
            {
                long frameStart = clock.ElapsedMilliseconds;
                var source = Source;
                Bitmap pic = null;
                string problem = "Pick a dash window or monitor";
                if (source != null) pic = source.Capture(out problem);
                if (pic == null)
                {
                    pic = Placeholder(problem);
                    if (problem != lastProblem) Status(problem);
                }
                else if (lastProblem != null)
                {
                    Status($"Streaming to Switch at {who}");
                }
                lastProblem = pic == placeholder ? problem : null;

                var frame = Scaler.Fit(pic, MaxWidth, MaxHeight);
                jpeg.SetLength(0);
                using (var p = new EncoderParameters(1))
                {
                    p.Param[0] = new EncoderParameter(Encoder.Quality, (long)Quality);
                    frame.Save(jpeg, encoder, p);
                }
                int len = (int)jpeg.Length;
                header[4] = (byte)len;
                header[5] = (byte)(len >> 8);
                header[6] = (byte)(len >> 16);
                header[7] = (byte)(len >> 24);
                stream.Write(header, 0, 8);
                stream.Write(jpeg.GetBuffer(), 0, len);
                if (stream.Read(ack, 0, 1) != 1) break;

                frames++;
                if (fpsClock.ElapsedMilliseconds >= 2000 && lastProblem == null)
                {
                    double fps = frames * 1000.0 / fpsClock.ElapsedMilliseconds;
                    Status($"Streaming to Switch at {who}: {frame.Width}x{frame.Height}, {fps:0} fps, {len / 1024} KB per frame");
                    frames = 0;
                    fpsClock.Restart();
                }

                int wait = (int)(1000 / Math.Max(1, Fps) - (clock.ElapsedMilliseconds - frameStart));
                if (wait > 0) Thread.Sleep(wait);
            }
        }

        private static Bitmap placeholder;
        private static string placeholderText;

        private static Bitmap Placeholder(string text)
        {
            if (placeholder != null && placeholderText == text) return placeholder;
            placeholder?.Dispose();
            placeholder = new Bitmap(1280, 720);
            placeholderText = text;
            using (var g = Graphics.FromImage(placeholder))
            using (var font = new Font("Segoe UI", 28))
            using (var fmt = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
            {
                g.Clear(Color.FromArgb(8, 9, 12));
                g.DrawString("SimDash Streamer\n\n" + text, font, Brushes.Orange, new RectangleF(40, 0, 1200, 720), fmt);
            }
            return placeholder;
        }
    }
}
