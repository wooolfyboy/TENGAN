# -*- coding: utf-8 -*-
"""
Radio Radar — монитор нагрузки ноутбука.

Раз в секунду смотрит загрузку процессора, память, диск, сеть и батарею
и отправляет это на палку по USB-кабелю. Палка показывает всё в разделе PC.

Обратно палка присылает команды пульта (страница «Media» в разделе PC):
  KEY VOLUP / KEY VOLDN — громче / тише, KEY PLAY — пауза/играть.
Программа нажимает за тебя мультимедийные клавиши Windows.

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

# ---------- мультимедийные клавиши (только Windows) ----------
IS_WIN = os.name == "nt"
VK = {"VOLUP": 0xAF, "VOLDN": 0xAE, "MUTE": 0xAD, "PLAY": 0xB3, "NEXT": 0xB0, "PREV": 0xB1}
if IS_WIN:
    import ctypes
    _user32 = ctypes.windll.user32


def press(name):
    """Нажать мультимедийную клавишу, как на клавиатуре ноутбука."""
    code = VK.get(name)
    if code is None or not IS_WIN:
        return
    _user32.keybd_event(code, 0, 0, 0)        # нажали
    _user32.keybd_event(code, 0, 2, 0)        # отпустили


# ---------- текущая громкость (если стоит библиотека pycaw; без неё палка просто не покажет %) ----------
_vol_iface = None


def read_volume():
    """Вернуть (громкость 0..100, без звука?) или None."""
    global _vol_iface
    if not IS_WIN:
        return None
    try:
        if _vol_iface is None:
            from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
            spk = AudioUtilities.GetSpeakers()
            if hasattr(spk, "EndpointVolume"):           # новые версии pycaw
                _vol_iface = spk.EndpointVolume
            else:                                        # старые версии pycaw
                from comtypes import CLSCTX_ALL
                from ctypes import POINTER, cast
                raw = spk.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
                _vol_iface = cast(raw, POINTER(IAudioEndpointVolume))
        level = _vol_iface.GetMasterVolumeLevelScalar()
        return int(round(level * 100)), bool(_vol_iface.GetMute())
    except Exception:
        _vol_iface = None
        return None


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
    s.timeout = 1             # чтение команд от палки: ждать строку не больше секунды
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


def reader(port, wake):
    """Отдельный поток: слушает команды от палки и сразу их выполняет."""
    while True:
        try:
            raw = port.readline()                # ждёт строку (до 1 с), потом снова
        except Exception:
            return                               # порт закрыли — поток заканчивается
        if not raw:
            continue
        line = raw.decode("ascii", "ignore").strip()
        if line.startswith("KEY "):
            name = line[4:].strip()
            press(name)
            label = {"VOLUP": "громче", "VOLDN": "тише", "PLAY": "пауза/играть"}.get(name, name)
            sys.stdout.write("\r     пульт: %-14s                              " % label)
            sys.stdout.flush()
            wake.set()                           # сразу отправить новую громкость


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
    wake = threading.Event()
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
                say("     Колесо громкости: в разделе PC прокрути колесом до страницы Media.")
                warned = False
                threading.Thread(target=reader, args=(port, wake), daemon=True).start()
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
            if wake.is_set():                    # только что крутили колесо — громкость сразу
                wake.clear()
                time.sleep(0.05)                 # Windows нужно мгновение, чтобы применить
                v = read_volume()
                if v:
                    port.write(("PV vol=%d mute=%d\n" % (v[0], 1 if v[1] else 0)).encode())
                wake.wait(max(0.0, next_t - time.monotonic()))   # и дальше ждём до своей секунды
                continue
            line, cpu, ram = col.line()
            port.write(line.encode("ascii", "replace"))
            v = read_volume()
            if v:
                port.write(("PV vol=%d mute=%d\n" % (v[0], 1 if v[1] else 0)).encode())
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
        wake.wait(wait)                          # проснуться раньше, если крутили колесо


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        say("")
        say("Остановлено.")
