# Made by @xdavidhu (github.com/xdavidhu, https://xdavidhu.me/)
# for Windows: add by @nmori (github.com/nmori/)
	
import serial
import io
import os
import subprocess
import signal
import time


print("--------------------------------------------------------")
print("Wireshark ワイヤレスモニタープロープ接続スクリプト v1.0a")
print("--------------------------------------------------------")
print("※Git for Win (Unixコマンド含)と、Python3系をインストールすれば動きます。")
print("※WiresharkのフォルダはPathを通しておいてください。")
print("--------------------------------------------------------")
print("<Windows動作版>\n")

try:
    serialportInput = input("[?] シリアルポートを指定 (初期値： 'COM6'): ")
    if serialportInput == "":
        serialport = "COM6"
    else:
        serialport = serialportInput
except KeyboardInterrupt:
    print("\n[+] 終了...")
    exit()

try:
    canBreak = False
    while not canBreak:
        boardRateInput = input("[?]通信レートを設定 (初期値:'921600'): ")
        if boardRateInput == "":
            boardRate = 921600
            canBreak = True
        else:
            try:
                boardRate = int(boardRateInput)
            except KeyboardInterrupt:
                print("\n[+] 終了...")
                exit()
            except Exception as e:
                print("[!] 数字を入れてください。(例：921600、115200など)!")
                continue
            canBreak = True
except KeyboardInterrupt:
    print("\n[+] 終了...")
    exit()

try:
    filenameInput = input("[?] 出力ファイル名を指定 (初期値: 'capture.pcap'): ")
    if filenameInput == "":
        filename = "capture.pcap"
    else:
        filename = filenameInput
except KeyboardInterrupt:
    print("\n[+] 終了...")
    exit()

canBreak = False
while not canBreak:
    try:
        ser = serial.Serial(serialport, boardRate)
        canBreak = True
    except KeyboardInterrupt:
        print("\n[+] 終了...")
        exit()
    except Exception as e:
        print("[!] 接続待ち...",e)
        time.sleep(2)
        continue

print("[+] 接続を確認しました。接続先: " + ser.name)
print("[!] M5Stackでプローブアプリを起動しCボタンを押してください。")
counter = 0
f = open(filename,'wb')
	
print("[+] 通信の頭出しを実施中...")

check = 0
while check == 0:
    line = ser.readline()
    if b"<<START>>" in line:
        check = 1
        print("[+] 通信の先頭を認識しました...")
    #else: print '"'+line+'"'

print("[+] wiresharkを起動します...")
cmd = "tail -f -c +0 " + filename + " | wireshark -k -i -"
p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                       shell=True)

try:
    while True:
        ch = ser.read()
        f.write(ch)
        f.flush()
except KeyboardInterrupt:
    print("[+] 停止...")
    os.killpg(os.getpgid(p.pid), signal.SIGTERM)

f.close()
ser.close()
print("[+] 完了.")
