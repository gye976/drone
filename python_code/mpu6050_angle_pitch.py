import socket
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import threading

# 타이틀 이름 변수 정의
TITLE_NAME = 'mpu6050 angle pitch'

DATA_NAME = 'mpu6050'

# 서버 설정
SERVER_IP = "0.0.0.0"  # 모든 네트워크 인터페이스에서 접근 가능
SERVER_PORT = 5007

# 데이터 포인트 인덱스를 지정합니다.
POINT_0_INDEX = 0
POINT_1_INDEX = 1  # 두 번째 데이터 포인트 (인덱스 1로 수정)

# 인덱스 정의 (쉼표로 구분된 데이터 포인트 내 인덱스)
index_0 = 0  # 첫 번째 데이터 포인트에서의 인덱스
index_1 = 0  # 두 번째 데이터 포인트에서의 인덱스

# 데이터 이름을 변수로 지정
DATA_NAME_0 = 'Target'
DATA_NAME_1 = 'Input'

# 데이터 버퍼 초기화 및 최대 크기 설정
data_buffer = []
BUFFER_SIZE = 1000  # 최대 데이터 포인트 수

# Cycle 추적을 위한 카운터 초기화
cycle_counter = 0

# UDP 소켓 생성 및 바인딩
server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_socket.bind((SERVER_IP, SERVER_PORT))

def receive_data():
    global data_buffer, cycle_counter
    while True:
        try:
            # 데이터 수신
            data, _ = server_socket.recvfrom(1024)
            data_str = data.decode('utf-8')
            
            # 수신된 데이터 출력
            print(f"수신된 데이터: {data_str}")

            # 데이터 파싱
            lines = data_str.strip().split('\n')  # 줄바꿈으로 여러 라인을 나눔

            for line in lines:
                if not line.startswith(DATA_NAME):
                    continue  

                line = line.split(DATA_NAME)[1]  
                parts = line.strip().split()
                if len(parts) < 2:  # 데이터 포인트가 2개이므로 길이 체크를 2로 수정
                    continue

                # 데이터 포인트를 공백으로 구분하여 가져오기
                data_points = parts  # 이전 형식과 다르게 이제 parts 전체가 데이터 포인트

                if len(data_points) > POINT_1_INDEX:  # 이제 두 개의 데이터 포인트 필요
                    # 0번째, 1번째 데이터 포인트 선택
                    selected_point_0 = data_points[POINT_0_INDEX]
                    selected_point_1 = data_points[POINT_1_INDEX]

                    # 데이터 포인트를 쉼표로 분리
                    values_0 = selected_point_0.split(',')
                    values_1 = selected_point_1.split(',')
                    
                    if len(values_0) == 3 and len(values_1) == 3:
                        try:
                            data_0 = float(values_0[index_0])
                            data_1 = float(values_1[index_1])
                            
                            # 추출된 데이터 출력
                            print(f"추출된 데이터: Cycle={cycle_counter}, {DATA_NAME_0}={data_0}, {DATA_NAME_1}={data_1}")
                            
                            # 데이터 버퍼에 추가
                            data_buffer.append({
                                'Cycle': cycle_counter,
                                DATA_NAME_0: data_0,
                                DATA_NAME_1: data_1
                            })
                            
                            # Cycle 카운터 증가
                            cycle_counter += 1
                            
                            # 데이터 버퍼 크기 제한
                            if len(data_buffer) > BUFFER_SIZE:
                                data_buffer.pop(0)
                        except ValueError:
                            continue
        except Exception as e:
            print(f"데이터 수신 오류: {e}")

# 데이터 수신을 위한 스레드 생성
receive_thread = threading.Thread(target=receive_data, daemon=True)
receive_thread.start()

# 데이터 처리 함수
def process_data(data_buffer):
    df = pd.DataFrame(data_buffer)
    return df

# 실시간 업데이트 함수
def update_plot(frame, line_0, line_1, ax):
    df = process_data(data_buffer)
    
    # 데이터가 비어있는지 확인
    if df.empty:
        return line_0, line_1

    # 데이터 업데이트
    line_0.set_data(df['Cycle'], df[DATA_NAME_0])
    line_1.set_data(df['Cycle'], df[DATA_NAME_1])
    
    # x축 범위 설정
    if not df.empty:
        current_min = df['Cycle'].min()
        current_max = df['Cycle'].max()
        
        # x축의 시작과 끝을 동적으로 설정
        ax.set_xlim(current_min, current_max)  # 데이터의 최소와 최대를 x축의 범위로 설정
        
        # x축 범위가 너무 좁아지는 것을 방지
        if current_max - current_min < 1:
            ax.set_xlim(current_min - 1, current_max + 1)  # 데이터가 너무 조밀할 때 여유를 추가

    # y축 범위 설정
    ax.set_ylim(-100, 100)
    
    return line_0, line_1

# 설정
fig, ax = plt.subplots()

# 선 그래프 초기화
line_0, = ax.plot([], [], lw=2, label=DATA_NAME_0, color='b')
line_1, = ax.plot([], [], lw=2, label=DATA_NAME_1, color='orange')

# 애니메이션 설정
ani = animation.FuncAnimation(
    fig, 
    update_plot, 
    fargs=(line_0, line_1, ax), 
    interval=100,  # 애니메이션 업데이트 간격을 100ms로 설정
    blit=False  # 성능 최적화
)

plt.legend()
plt.xlabel('Cycle')
plt.ylabel('Value')
plt.title(TITLE_NAME)  # 타이틀 이름 변수 사용
plt.show()

