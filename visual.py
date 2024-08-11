import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# 파일 경로
FILE_PATH = 'mpu6050.txt'
# 데이터 포인트 인덱스를 지정합니다.
POINT_0_INDEX = 0
POINT_1_INDEX = 1

index_0 = 1 #쉼표 내 인덱스
index_1 = 1 #쉼표 내 인덱스

# 데이터 읽기 함수
def read_data(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()
    return data

# 데이터 처리 함수
def process_data(data, point_0_index, point_1_index):
    rows = []
    for line in data:
        parts = line.strip().split()
        
        if len(parts) < 2:
            continue
        
        cycle = int(parts[0])
        
        # 데이터 포인트를 공백으로 구분하여 가져오기
        data_points = parts[1:]  # parts[1:]는 모든 데이터 포인트를 포함

        if len(data_points) > point_1_index:
            # 0번째 데이터 포인트와 1번째 데이터 포인트 선택
            selected_point_0 = data_points[point_0_index]
            selected_point_1 = data_points[point_1_index]

            # 데이터 포인트를 쉼표로 분리
            values_0 = selected_point_0.split(',')
            values_1 = selected_point_1.split(',')
            
            if len(values_0) == 3 and len(values_1) == 3:
                try:
                    target_value = float(values_0[index_0])
                    input_value = float(values_1[index_1])
                except ValueError:
                    continue

                row = {
                    'Cycle': cycle,
                    'Target': target_value,
                    'Input': input_value,
                }
                rows.append(row)
    
    df = pd.DataFrame(rows)
    return df

# 실시간 업데이트 함수
def update_plot(frame, file_path, line_target, line_input, ax):
    data = read_data(file_path)
    df = process_data(data, POINT_0_INDEX, POINT_1_INDEX)
    
    # 데이터가 비어있는지 확인
    if df.empty:
        return line_target, line_input

    # 데이터 업데이트
    line_target.set_data(df['Cycle'], df['Target'])
    line_input.set_data(df['Cycle'], df['Input'])
    
    # x축 범위 설정
    current_min = df['Cycle'].min()
    current_max = df['Cycle'].max()
    if current_max > 3000:
        ax.set_xlim(current_max - 3000, current_max)
    else:
        ax.set_xlim(0, 3000)
        
    ax.set_ylim(-90, 90)  # y축 범위를 -90도에서 90도까지로 설정
    
    return line_target, line_input

# 데이터 파일 경로 설정
file_path = FILE_PATH

# 설정
fig, ax = plt.subplots()
ax.set_xlim(0, 3000)  # x축 범위를 3000 사이클로 설정
ax.set_ylim(-90, 90)  # y축 범위를 -90도에서 90도까지로 설정

# 선 그래프 초기화
line_target, = ax.plot([], [], lw=2, label='Target')
line_input, = ax.plot([], [], lw=2, label='Input')

# 애니메이션 설정
ani = animation.FuncAnimation(
    fig, 
    update_plot, 
    fargs=(file_path, line_target, line_input, ax), 
    interval=500,  # 애니메이션 업데이트 간격을 500ms로 설정
    blit=False
)

plt.legend()
plt.xlabel('Cycle')
plt.ylabel('Value')
plt.title('Real-time Data Visualization')
plt.show()

