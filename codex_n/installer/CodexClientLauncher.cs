using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

internal static class CodexClientLauncher
{
    private const int DefaultPort = 3790;

    [STAThread]
    private static int Main()
    {
        string appDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string dataDir = Path.Combine(appDir, ".codex-client");
        Directory.CreateDirectory(dataDir);

        Dictionary<string, string> config = ReadConfig(Path.Combine(appDir, "launcher.ini"));
        int port = ParsePort(config);
        string workspace = ResolveWorkspace(config, appDir);
        string url = "http://127.0.0.1:" + port + "/";
        string healthUrl = url + "api/health";

        if (IsHealthy(healthUrl))
        {
            OpenUrl(url);
            return 0;
        }

        string nodePath = FindOnPath("node.exe");
        if (String.IsNullOrEmpty(nodePath))
        {
            MessageBox.Show("找不到 node.exe。请确认 Node.js 已安装并在 PATH 中。", "Codex Client", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }

        string stdoutLog = Path.Combine(dataDir, "server.out.log");
        string stderrLog = Path.Combine(dataDir, "server.err.log");
        string pidFile = Path.Combine(dataDir, "server.pid");

        Process process;
        try
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = nodePath,
                Arguments = "server.js",
                WorkingDirectory = appDir,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            startInfo.EnvironmentVariables["PORT"] = port.ToString();
            startInfo.EnvironmentVariables["CODEX_CLIENT_PORT"] = port.ToString();
            startInfo.EnvironmentVariables["CODEX_CLIENT_WORKSPACE"] = workspace;
            startInfo.EnvironmentVariables["FORCE_COLOR"] = "0";

            string codexPath = FindCodexCommand();
            if (!String.IsNullOrEmpty(codexPath))
            {
                startInfo.EnvironmentVariables["CODEX_COMMAND"] = codexPath;
                PrependPath(startInfo, Path.GetDirectoryName(codexPath));
            }

            process = Process.Start(startInfo);
            if (process == null)
            {
                throw new InvalidOperationException("无法启动 node 进程。");
            }

            File.WriteAllText(pidFile, process.Id.ToString());
            process.OutputDataReceived += delegate(object sender, DataReceivedEventArgs args)
            {
                AppendLog(stdoutLog, args.Data);
            };
            process.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs args)
            {
                AppendLog(stderrLog, args.Data);
            };
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
        }
        catch (Exception ex)
        {
            MessageBox.Show("启动 Codex Client 失败：" + ex.Message, "Codex Client", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }

        string actualUrl = WaitForServer(port, healthUrl, stdoutLog);
        if (String.IsNullOrEmpty(actualUrl))
        {
            string detail = File.Exists(stderrLog) ? File.ReadAllText(stderrLog) : "";
            if (detail.Length > 1200)
            {
                detail = detail.Substring(detail.Length - 1200);
            }
            MessageBox.Show("服务没有正常启动。\n\n" + detail, "Codex Client", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }

        OpenUrl(actualUrl);

        while (!process.HasExited)
        {
            Thread.Sleep(2000);
        }

        return process.ExitCode;
    }

    private static Dictionary<string, string> ReadConfig(string path)
    {
        Dictionary<string, string> config = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(path))
        {
            return config;
        }

        foreach (string rawLine in File.ReadAllLines(path))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith("#"))
            {
                continue;
            }

            int split = line.IndexOf('=');
            if (split <= 0)
            {
                continue;
            }

            config[line.Substring(0, split).Trim()] = line.Substring(split + 1).Trim();
        }

        return config;
    }

    private static int ParsePort(Dictionary<string, string> config)
    {
        string value;
        int port;
        if (config.TryGetValue("port", out value) && Int32.TryParse(value, out port) && port > 0 && port < 65536)
        {
            return port;
        }

        return DefaultPort;
    }

    private static string ResolveWorkspace(Dictionary<string, string> config, string appDir)
    {
        string workspace;
        if (config.TryGetValue("workspace", out workspace) && Directory.Exists(workspace))
        {
            return Path.GetFullPath(workspace);
        }

        string driveRoot = Path.GetPathRoot(appDir);
        string candidate = Path.Combine(driveRoot ?? appDir, "codex_n");
        return Directory.Exists(candidate) ? candidate : appDir;
    }

    private static bool IsHealthy(string healthUrl)
    {
        try
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(healthUrl);
            request.Timeout = 900;
            request.ReadWriteTimeout = 900;
            using (HttpWebResponse response = (HttpWebResponse)request.GetResponse())
            {
                return response.StatusCode == HttpStatusCode.OK;
            }
        }
        catch
        {
            return false;
        }
    }

    private static string WaitForServer(int port, string healthUrl, string stdoutLog)
    {
        string defaultUrl = "http://127.0.0.1:" + port + "/";
        for (int attempt = 0; attempt < 80; attempt++)
        {
            if (IsHealthy(healthUrl))
            {
                return defaultUrl;
            }

            string loggedUrl = TryReadLoggedUrl(stdoutLog);
            if (!String.IsNullOrEmpty(loggedUrl) && IsHealthy(loggedUrl + "api/health"))
            {
                return loggedUrl;
            }

            Thread.Sleep(150);
        }

        return null;
    }

    private static string TryReadLoggedUrl(string stdoutLog)
    {
        try
        {
            if (!File.Exists(stdoutLog))
            {
                return null;
            }

            string text = File.ReadAllText(stdoutLog);
            Match match = Regex.Match(text, @"http://127\.0\.0\.1:\d+/?");
            if (match.Success)
            {
                string value = match.Value;
                return value.EndsWith("/") ? value : value + "/";
            }
        }
        catch
        {
        }

        return null;
    }

    private static string FindOnPath(string fileName)
    {
        string pathValue = Environment.GetEnvironmentVariable("PATH") ?? "";
        foreach (string directory in pathValue.Split(Path.PathSeparator))
        {
            try
            {
                if (String.IsNullOrWhiteSpace(directory))
                {
                    continue;
                }

                string candidate = Path.Combine(directory.Trim(), fileName);
                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
            catch
            {
            }
        }

        return null;
    }

    private static string FindCodexCommand()
    {
        string codex = FindOnPath("codex.cmd");
        if (!String.IsNullOrEmpty(codex))
        {
            return codex;
        }

        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string npmCodex = Path.Combine(appData, "npm", "codex.cmd");
        return File.Exists(npmCodex) ? npmCodex : null;
    }

    private static void PrependPath(ProcessStartInfo startInfo, string directory)
    {
        if (String.IsNullOrEmpty(directory))
        {
            return;
        }

        string current = startInfo.EnvironmentVariables["PATH"] ?? "";
        if (current.IndexOf(directory, StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return;
        }

        startInfo.EnvironmentVariables["PATH"] = directory + Path.PathSeparator + current;
    }

    private static void OpenUrl(string url)
    {
        Process.Start(new ProcessStartInfo(url) { UseShellExecute = true });
    }

    private static void AppendLog(string path, string line)
    {
        if (line == null)
        {
            return;
        }

        try
        {
            File.AppendAllText(path, line + Environment.NewLine);
        }
        catch
        {
        }
    }
}
