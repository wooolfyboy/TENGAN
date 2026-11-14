# -*- coding: utf-8 -*-
"""
Radio Radar — монитор нагрузки ноутбука.

Раз в секунду смотрит загрузку процессора, память, диск, сеть и батарею
и отправляет это на палку по USB-кабелю. Палка показывает всё в разделе PC.

Запускать двойным кликом по start_pc_monitor.bat (он сам всё поставит).
Остановить — закрыть окно или нажать Ctrl+C.
"""
import os
import socket
import sys
import threading
import time

try:
    import psutil
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Не хватает библиотек. Запусти программу через start_pc_monitor.bat — он их поставит.")
    input("Нажми Enter, чтобы закрыть окно...")
    sys.exit(1)

ESP_VID = 0x303A          # «код производителя» USB у чипов Espressif (наша палка)


def say(msg=""):
    print(msg, flush=True)


def find_port(forced=None):
    """Найти палку среди USB-устройств. Вернуть имя порта (например COM5) или None."""
    if forced:
        return forced
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.vid == ESP_VID:
            return p.device
    for p in ports:                                   # запасной вариант — по названию
        d = (p.description or "").lower()
        if "jtag" in d or "esp32" in d:
            return p.device
    return None


def open_port(name):
    s = serial.Serial()
    s.port = name
    s.baudrate = 115200
    s.dtr = False             # иначе палка может перезагрузиться при подключении
    s.rts = False
    s.write_timeout = 2
    s.open()
    return s


def clean(text, n=20):
    """Убрать пробелы и лишнее из имени (палка делит строку по пробелам)."""
    text = "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in (text or ""))
    return text[:n] or "-"


class Collector:
    def __init__(self):
        psutil.cpu_percent(interval=None)
        psutil.cpu_percent(interval=None, percpu=True)
        self.t = time.time()
        self.disk = psutil.disk_io_counters()
        self.net = psutil.net_io_counters()
        self.top = "-"
        self.procs = {}
        # поиск «прожорливого» процесса — в отдельном потоке: на Windows перебор
        # сотен процессов занимает 1-2 с и иначе задерживал бы отправку
        threading.Thread(target=self._top_loop, daemon=True).start()
        self.root = "C:\\" if os.name == "nt" else "/"
        self.host = clean(socket.gethostname())

    def _top_loop(self):
        while True:
            try:
                self._find_top()
            except Exception:
                pass
            time.sleep(4)

    def _find_top(self):
        """Самый «прожорливый» процесс."""
        best, best_cpu = "-", -1.0
        seen = {}
        for p in psutil.process_iter(["name"]):
            try:
                if p.pid == 0:
                    continue              # «System Idle Process» — это простой, а не нагрузка
                proc = self.procs.get(p.pid) or p
                cpu = proc.cpu_percent(interval=None)
                seen[p.pid] = proc
                if cpu > best_cpu:
                    best, best_cpu = p.info["name"] or "-", cpu
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        self.procs = seen
        self.top = clean(best.replace(".exe", ""))

    def line(self):
        now = time.time()
        dt = max(0.2, now - self.t)
        self.t = now
        cpu = psutil.cpu_percent(interval=None)
        cores = psutil.cpu_percent(interval=None, percpu=True)[:16]
        try:
            freq = (psutil.cpu_freq().current or 0) / 1000.0
        except Exception:
            freq = 0
        vm = psutil.virtual_memory()
        try:
            du = psutil.disk_usage(self.root).percent
        except Exception:
            du = 0
        d = psutil.disk_io_counters()
        dr = dw = 0.0
        if d and self.disk:
            dr = (d.read_bytes - self.disk.read_bytes) / dt / 1048576
            dw = (d.write_bytes - self.disk.write_bytes) / dt / 1048576
        self.disk = d
        n = psutil.net_io_counters()
        nd = (n.bytes_recv - self.net.bytes_recv) / dt / 1024
        nu = (n.bytes_sent - self.net.bytes_sent) / dt / 1024
        self.net = n
        bat = psutil.sensors_battery() if hasattr(psutil, "sensors_battery") else None
        up = int(now - psutil.boot_time())
        parts = [
            "PC",
            "cpu=%.1f" % cpu,
            "freq=%.2f" % freq,
            "c=" + ",".join(str(int(round(c))) for c in cores),
            "ram=%.1f" % vm.percent,
            "ramu=%.1f" % ((vm.total - vm.available) / 1073741824),
            "ramt=%.1f" % (vm.total / 1073741824),
            "dsk=%.0f" % du,
            "dr=%.1f" % max(0, dr),
            "dw=%.1f" % max(0, dw),
            "nd=%.0f" % max(0, nd),
            "nu=%.0f" % max(0, nu),
            "up=%d" % up,
            "top=" + self.top,
            "host=" + self.host,
        ]
        if bat is not None:
            parts += ["bat=%d" % bat.percent, "plug=%d" % (1 if bat.power_plugged else 0)]
        return " ".join(parts) + "\n", cpu, vm.percent


def main():
    forced = None
    if len(sys.argv) > 2 and sys.argv[1] == "--port":
        forced = sys.argv[2]
    say("=" * 56)
    say("  Radio Radar — монитор ноутбука")
    say("  Окно не закрывай: пока оно открыто, палка получает данные.")
    say("  Остановить — закрыть окно (или Ctrl+C).")
    say("=" * 56)
    col = Collector()
    port = None
    warned = False
    next_t = time.monotonic()
    while True:
        # 1. найти и открыть палку
        if port is None:
            name = find_port(forced)
            if not name:
                if not warned:
                    say("")
                    say("[..] Ищу палку... Подключи её к ноуту USB-кабелем и включи.")
                    say("     (Ищу дальше сама — ничего нажимать не надо.)")
                    warned = True
                time.sleep(2)
                continue
            try:
                port = open_port(name)
                say("")
                say("[OK] Палка найдена: %s. Отправляю данные. Открой на палке раздел PC." % name)
                warned = False
            except Exception as e:
                text = str(e).lower()
                if "denied" in text or "busy" in text or "permission" in text or "отказано" in text:
                    say("[!!] Порт %s занят другой программой." % name)
                    say("     Закрой сайт прошивки (esptool-js) и другие окна этой программы.")
                else:
                    say("[!!] Не получилось открыть %s: %s" % (name, e))
                time.sleep(3)
                continue
        # 2. отправить показатели
        try:
            line, cpu, ram = col.line()
            port.write(line.encode("ascii", "replace"))
            sys.stdout.write("\r     CPU %3d%%   RAM %3d%%   -> %s   " % (cpu, ram, port.port))
            sys.stdout.flush()
        except Exception:
            say("")
            say("[..] Связь с палкой пропала (кабель отключили?). Жду, когда вернётся...")
            try:
                port.close()
            except Exception:
                pass
            port = None
        # ровно раз в секунду, сколько бы ни заняли замеры
        next_t += 1.0
        wait = next_t - time.monotonic()
        if wait < 0:
            next_t = time.monotonic()
            wait = 0
        time.sleep(wait)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        say("")
        say("Остановлено.")
