using BatteryIndicator.Properties;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BatteryIndicator
{
    static class Program
    {
        const float BATTERY_LOW = 3.6f;
        const float BATTERY_HIGH = 4.252f;

        static Dictionary<int, Icon> batteryIcons = new Dictionary<int, Icon>()
        {
            { 0, Resources.battery_dead },
            { 1, Resources.battery_10 },
            { 2, Resources.battery_20 },
            { 3, Resources.battery_30 },
            { 4, Resources.battery_40 },
            { 5, Resources.battery_50 },
            { 6, Resources.battery_60 },
            { 7, Resources.battery_70 },
            { 8, Resources.battery_80 },
            { 9, Resources.battery_90 },
            { 10, Resources.battery_100 },
        };

        static Dictionary<int, Icon> batteryIconsCharging = new Dictionary<int, Icon>()
        {
            { 0, Resources.battery_charging_10 },
            { 1, Resources.battery_charging_10 },
            { 2, Resources.battery_charging_20 },
            { 3, Resources.battery_charging_30 },
            { 4, Resources.battery_charging_40 },
            { 5, Resources.battery_charging_50 },
            { 6, Resources.battery_charging_60 },
            { 7, Resources.battery_charging_70 },
            { 8, Resources.battery_charging_80 },
            { 9, Resources.battery_charging_90 },
            { 10, Resources.battery_charging_100 },
        };

        private static NotifyIcon notifyIcon;

        private static Thread threadSerialReader;
        private static SerialPort serialPort;

        private static Thread threadIconHelper;

        private static HeadphoneFlags headphoneFlags;
        private static float headphoneBatteryVoltage;

        private static bool showFlagsIcon = false;

        [STAThread]
        static void Main()
        {
            try
            {
                Config config = JsonConvert.DeserializeObject<Config>(File.ReadAllText("config.json"));

                notifyIcon = new NotifyIcon();
                notifyIcon.Icon = Resources.battery_charging_60;

                notifyIcon.DoubleClick += NotifyIcon_DoubleClick;

                notifyIcon.Visible = true;

                serialPort = new SerialPort(config.Port, config.Baud);
                serialPort.Open();
                serialPort.DtrEnable = true;

                threadSerialReader = new Thread(Thread_SerialRead);
                threadSerialReader.IsBackground = true;
                threadSerialReader.Start();

                threadIconHelper = new Thread(Threa_IconHelper);
                threadIconHelper.IsBackground = true;
                threadIconHelper.Start();

                Application.Run();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private static void NotifyIcon_DoubleClick(object sender, EventArgs e)
        {
            Application.Exit();
        }

        static int MapVoltageToPercent(float voltage)
        {
            int value = (int)Math.Floor(((voltage - BATTERY_LOW) / (BATTERY_HIGH - BATTERY_LOW)) * 100);

            if (value < 0) value = 0;
            if (value > 100) value = 100;

            return value;
        }

        static void UpdateIcon()
        {
            int batteryPercent = MapVoltageToPercent(headphoneBatteryVoltage);

            notifyIcon.Text = $"{batteryPercent}% ({headphoneBatteryVoltage.ToString("F")}V){(headphoneFlags.HasFlag(HeadphoneFlags.Charging) ? " Charging": "")}{(headphoneFlags.HasFlag(HeadphoneFlags.Mute) ? " Muted" : "")}";

            bool showBatteryIcon = true;
            if (showFlagsIcon)
            {
                if (headphoneFlags.HasFlag(HeadphoneFlags.Mute))
                {
                    showBatteryIcon = false;

                    notifyIcon.Icon = Resources.mute;
                }
            }

            if (showBatteryIcon)
            {
                int batteryKey = (batteryPercent / 10);
                if (headphoneFlags.HasFlag(HeadphoneFlags.Charging))
                {
                    notifyIcon.Icon = batteryIconsCharging[batteryKey];
                }
                else
                {
                    notifyIcon.Icon = batteryIcons[batteryKey];
                }
            }
        }

        static void Threa_IconHelper()
        {
            while (true)
            {
                Thread.Sleep(1000);

                showFlagsIcon = !showFlagsIcon;

                UpdateIcon();
            }
        }

        static void Thread_SerialRead()
        {
            serialPort.Write(new byte[] { 0xAF }, 0, 1); // Begin command
            serialPort.BaseStream.Flush();

            while (true)
            {
                if (serialPort.BytesToRead >= 2)
                {
                    int header = serialPort.ReadByte();
                    if (header != 0xA5) // Check packet header
                    {
                        continue;
                    }

                    int dataLength = serialPort.ReadByte();
                    if (dataLength < 0 || dataLength > 128) // Check for nonsense sizes
                    {
                        continue;
                    }

                    byte[] dataBytes = new byte[dataLength];
                    int readCount = serialPort.Read(dataBytes, 0, dataLength);

                    if (readCount != dataLength) // Make sure enough data is read
                    {
                        continue;
                    }

                    if (dataBytes[0] != 0x02 || dataBytes[5] != 0x59 || dataBytes[6] != 0x00) // Check BLE advertisement header and device type
                    {
                        continue;
                    }

                    // Process packet
                    byte packetHeader = dataBytes[7];
                    if (packetHeader != 0x61) // Packet version 1, length 6 bytes
                    {
                        continue;
                    }

                    byte packetFlags = dataBytes[8]; // 0b 0000 00XY where X=charging, Y=mute

                    int readVoltageNumber = BitConverter.ToInt16(dataBytes, 11);
                    float batteryVoltage = (readVoltageNumber / 1000.0f);

                    // Store and show data
                    headphoneFlags = (HeadphoneFlags)packetFlags;
                    headphoneBatteryVoltage = batteryVoltage;

                    UpdateIcon();
                }
            }
        }
    }

    [Flags]
    enum HeadphoneFlags : byte
    {
        None = 0,
        Mute = 1,
        Charging = 2
    }

    class Config
    {
        [JsonProperty("port")]
        public string Port { get; private set; }

        [JsonProperty("baud")]
        public int Baud { get; private set; }
    }
}
