import socket

# 서버 설정
UDP_IP = "0.0.0.0"  # 모든 네트워크 인터페이스에서 수신
UDP_PORT = 5005     # 수신할 포트 번호

# 소켓 생성
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"UDP 서버가 {UDP_IP}:{UDP_PORT}에서 대기 중입니다...")

while True:
    # 데이터 수신
    data, addr = sock.recvfrom(1024)  # 버퍼 크기를 1024로 설정

    # 수신된 데이터 출력
    print(f"수신된 메시지: {data.decode('utf-8')} from {addr}")

