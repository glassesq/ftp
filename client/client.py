import socket

import argparse

import threading

# arguments parser
arg_parser = argparse.ArgumentParser(description='FTP shell client')
arg_parser.add_argument(
    '--port', type=int, help='The port of FTP server', required=True)
# arg_parser.add_argument('--sum', dest='accumulate', action='store_const',
# const=sum, default=max,
# help='sum the integers (default: find the max)')

args = arg_parser.parse_args()


def testReceive(s: socket):
    body = b''
    response_stream_ended = False
    while not response_stream_ended:
        data = s.recv(65536 * 1024)
        if not data:
            break
        print('data:', data)
        body += data
    print(body)
    print("end receive")


if __name__ == '__main__':
    print("welcome to client")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print("port:", args.port)
    s.connect(('0.0.0.0', args.port))
    thread = threading.Thread(target=testReceive, args=(s, ))
    thread.start()
    while(1):
        a = input()
        a = a.removesuffix("\n")
        s.send(a.encode() + b'\r\n')
        print('send:[%s]' % a)
   # s.send(b' RETR a.txt\r\n')
   # s.send(b'A\r\n');
   # s.send(b' USER anonymou s\r\n')
   # s.send(b' PASS aksjdhflkjashflka\r\n')
   # s.send(b' PASV\r\n')
   # s.send(b'RETR config.c\r\n')
