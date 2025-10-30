import subprocess
import sys
import tkinter as tk
from tkinter import Toplevel, Label, Button
import requests
import serial.tools.list_ports
import os
import threading
import uuid
import time


def resource_path(relative_path):
    if getattr(sys, 'frozen', False):
        base_path = sys._MEIPASS
    else:
        base_path = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base_path, relative_path)


data_directory = os.path.join(os.getenv('APPDATA'), 'qESP-H4CK')
os.makedirs(data_directory, exist_ok=True)


def center_window_over_main(window, width, height):
    main_x = root.winfo_x()
    main_y = root.winfo_y()
    main_width = root.winfo_width()
    main_height = root.winfo_height()

    x = main_x + (main_width - width) // 2
    y = main_y + (main_height - height) // 2
    return f"{width}x{height}+{x}+{y}"


def show_error(message):
    error_window = Toplevel(root)
    error_window.title("Error")
    error_window.configure(bg="#050403")
    error_window.resizable(False, False)
    error_window.geometry(center_window_over_main(error_window, 400, 150))

    label = Label(error_window, text=message, bg="#050403", fg="#ffffff", font=("Fixedsys", 14), wraplength=350)
    label.pack(pady=20)

    ok_button = Button(error_window, text="OK", command=error_window.destroy,
                       bg="#1c1c1c", fg="#ffffff", font=("Fixedsys", 12), borderwidth=2, relief="solid")
    ok_button.pack(pady=10)
    ok_button.bind("<Enter>", lambda e: ok_button.config(bg="white", fg="#1c1c1c"))
    ok_button.bind("<Leave>", lambda e: ok_button.config(bg="#1c1c1c", fg="#ffffff"))


def show_info(title, message):
    info_window = Toplevel(root)
    info_window.title(title)
    info_window.configure(bg="#050403")
    info_window.resizable(False, False)
    info_window.geometry(center_window_over_main(info_window, 400, 150))

    label = Label(info_window, text=message, bg="#050403", fg="#ffffff", font=("Fixedsys", 14), wraplength=350)
    label.pack(pady=20)

    ok_button = Button(info_window, text="OK", command=info_window.destroy,
                       bg="#1c1c1c", fg="#ffffff", font=("Fixedsys", 12), borderwidth=2, relief="solid")
    ok_button.pack(pady=10)
    ok_button.bind("<Enter>", lambda e: ok_button.config(bg="white", fg="#1c1c1c"))
    ok_button.bind("<Leave>", lambda e: ok_button.config(bg="#1c1c1c", fg="#ffffff"))


def get_latest_firmware_info():
    try:
        response = requests.get("https://api.github.com/repos/Teapot174/ESP-HACK/releases/latest")
        response.raise_for_status()
        release = response.json()
        for asset in release['assets']:
            if asset['name'].endswith('.bin'):
                return asset['browser_download_url'], asset['name'], release['name']
        raise Exception("No .bin file found in the latest release.")
    except Exception as e:
        show_error(f"Failed to fetch latest release: {e}")
        return None, None, None


# Установка bin'а
def install_firmware():
    try:
        firmware_url, firmware_name, _ = get_latest_firmware_info()
        if not firmware_url:
            return
        firmware_path = os.path.join(data_directory, "Firmware.bin")
        response = requests.get(firmware_url)
        response.raise_for_status()
        with open(firmware_path, 'wb') as f:
            f.write(response.content)
        flash_firmware(firmware_path, firmware_name)
    except Exception as e:
        show_error(f"Failed to download the firmware: {e}")


# Прошивка
def flash_firmware(firmware_path, firmware_name):
    if not selected_com_port:
        show_error("No COM port selected.")
        unblock_buttons()
        return
    esptool_path = resource_path('esptool.exe')

    loading_window = Toplevel(root)
    loading_window.title("qESP-H4CK")
    loading_window.configure(bg="#050403")
    loading_window.resizable(False, False)
    loading_window.geometry(center_window_over_main(loading_window, 300, 100))

    loading_label = Label(loading_window, text="Flashing...", bg="#050403", fg="#ffffff", font=("Fixedsys", 16))
    loading_label.pack(pady=20)

    def flash_device():
        try:
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = subprocess.SW_HIDE

            command = [esptool_path, '--port', selected_com_port, '--baud', '921600', 'write_flash', '0x0',
                       firmware_path]
            process = subprocess.run(command, check=True, capture_output=True, text=True, startupinfo=startupinfo)
            with open(os.path.join(data_directory, "flash_log.txt"), "w") as log_file:
                log_file.write(process.stdout)
            with open(os.path.join(data_directory, "firmware_log.txt"), "w") as firmware_log:
                firmware_log.write(firmware_name)
            show_info("qESP-H4CK", "The firmware is installed")
        except subprocess.CalledProcessError as e:
            with open(os.path.join(data_directory, "flash_error.log"), "w") as error_file:
                error_file.write(e.stderr)
            show_error("Failed to flash the device\nHold the BOOT button.")
        finally:
            loading_window.destroy()
            unblock_buttons()
            update_button_label()

    threading.Thread(target=flash_device).start()


def start_installation():
    block_buttons()
    install_firmware()


def block_buttons():
    install_button.config(state=tk.DISABLED, bg="gray", fg="white")


def unblock_buttons():
    install_button.config(state=tk.NORMAL, bg="#1c1c1c", fg="#ffffff")


def update_button_label():
    _, latest_firmware_name, latest_release_name = get_latest_firmware_info()
    firmware_log_path = os.path.join(data_directory, "firmware_log.txt")

    if latest_firmware_name:
        if os.path.exists(firmware_log_path):
            with open(firmware_log_path, "r") as firmware_log:
                current_version = firmware_log.read().strip()
            if latest_firmware_name != current_version:
                install_button.config(text="UPDATE")
                update_label.config(text=f"Update: {latest_release_name}")
            else:
                install_button.config(text="Install")
                update_label.config(text="No updates")
        else:
            install_button.config(text="UPDATE")
            update_label.config(text=f"Update: {latest_release_name}")
    else:
        install_button.config(text="Install")
        update_label.config(text="No updates")


def scan_com_ports():
    global selected_com_port

    def is_esp_port(port):
        esp_identifiers = [
            "10C4:EA60",  # Silabs CP2102
            "1A86:7523",  # CH340/CH341
            "0403:6001",  # FTDI FT232R
            "2341:0043",  # Arduino with ESP
        ]
        description = port.description.lower()
        vid_pid = f"{port.vid:04X}:{port.pid:04X}" if port.vid and port.pid else ""
        return any(esp_id in vid_pid for esp_id in esp_identifiers) or \
            any(keyword in description for keyword in ["esp", "ch340", "ch341", "cp2102", "usb serial"])

    def check_ports():
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            if is_esp_port(port):
                global selected_com_port
                selected_com_port = port.device
                show_main_interface()
                update_button_label()
                return
        root.after(500, check_ports)

    img.configure(image=connect_image)
    check_ports()


def show_main_interface():
    img.configure(image=background_image)
    install_button.place(relx=0.72, rely=0.80, anchor='center')
    update_label.place(relx=0.72, rely=0.92, anchor='center')


root = tk.Tk()
root.title("qESP-H4CK | v1.0")
root.configure(bg="#050403")
root.geometry("600x350")
root.resizable(False, False)

selected_com_port = None
connect_image = tk.PhotoImage(file=resource_path("Connect.png"))
background_image = tk.PhotoImage(file=resource_path("Background.png"))
img = Label(root, image=connect_image, bg="#050403")
img.place(relx=0.5, rely=0.0, anchor='n')

icon_image = tk.PhotoImage(file=resource_path("icon.png"))
root.iconphoto(True, icon_image)

install_button = Button(root, text="Install", command=lambda: threading.Thread(target=start_installation).start(),
                        bg="#1c1c1c", fg="#ffffff", borderwidth=2, relief="solid",
                        highlightbackground="#080808", highlightcolor="white", font=("Fixedsys", 20))

update_label = Label(root, text="Checking for updates...", fg="#ffffff", font=("Fixedsys", 12), bg="#050403", bd=0,
                     highlightthickness=0)

install_button.bind("<Enter>", lambda e: install_button.config(bg="white", fg="#1c1c1c", highlightbackground="#080808"))
install_button.bind("<Leave>", lambda e: install_button.config(bg="#1c1c1c", fg="#ffffff"))

scan_com_ports()

root.mainloop()