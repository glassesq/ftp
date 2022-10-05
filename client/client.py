import socket


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


if __name__ == '__main__':
    print("welcome to client")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 5001))
    s.send(b' USER anonymous\r\n')
    s.send(b' PSS aksjdhflkjashflka\r\n')
    testReceive(s)
    
