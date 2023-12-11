import matplotlib.pyplot as plt
import numpy as np

# 设置Nvidia风格的Matplotlib样式
plt.style.use({
    'axes.edgecolor': '#212121',
    'axes.facecolor': '#303030',
    'axes.labelcolor': 'white',
    'figure.facecolor': '#303030',
    'text.color': 'white',
    'xtick.color': 'white',
    'ytick.color': 'white',
    'grid.color': '#424242',
    'grid.linestyle': '--',
    'legend.facecolor': '#303030',
    'legend.edgecolor': '#212121',
    'legend.labelcolor': 'white'
})

plt.rcParams['font.family'] = 'sans-serif'
plt.rcParams['font.sans-serif'] = ['NVIDIA Corporation', 'Arial', 'Helvetica', 'DejaVu Sans']
plt.rcParams['font.size'] = 12

def create_nvidia_style_chart(data, data_names, labels, title="Nvidia-Style Chart", x_label="X-axis", y_label="Y-axis", color='skyblue', figsize=(10, 6)):
    """
    创建一个类似Nvidia风格的图表。

    参数:
    - data: 包含图表数据的列表或数组。
    - labels: 数据点的标签。
    - title: 图表标题。
    - x_label: X轴标签。
    - y_label: Y轴标签。
    - color: 数据线的颜色。
    - figsize: 图表的尺寸。

    返回:
    - 无返回值，直接显示图表。
    """
    # 设置图表样式
    plt.figure(figsize=figsize)
    plt.ylim(0, 500)
    # 隐藏 x 轴上的小竖线
    plt.tick_params(axis='x', which='both', bottom=False)


    # 定义Nvidia风格的颜色
    nvidia_colors = ['#FBAE17', '#00A0D1', '#76B900']

    # 绘制柱状图
    # bars = plt.bar(labels, data, color=nvidia_colors, edgecolor='black')
    
    bar_width = 0.3
    index = np.arange(len(labels))
    bars = []
    for i in range(len(data_names)):
        print(index + i * bar_width)
        print(data[i])
        bars.append(plt.bar(index + i * bar_width, data[i], bar_width, label=labels[i], color=nvidia_colors[i], edgecolor='black'))

    # 添加数值标签
    for i in range(len(bars)):
        bar = bars[i]
        for b in bar:
            yval = b.get_height()
            yText = round(yval, 2)
            if yval == 0:
                yText = "< 1"
            if (i == 2):
                plt.text(b.get_x() + b.get_width()/2, yval, yText, ha='center', va='bottom', color='#76B900')
            else:
                plt.text(b.get_x() + b.get_width()/2, yval, yText, ha='center', va='bottom', color='white')

    # 添加标题和标签
    plt.title(title, fontsize=16, color='white')
    plt.xlabel(x_label, fontsize=12, color='white')
    plt.ylabel(y_label, fontsize=12, color='white')

    bar_centers = index + 0.5 * (len(data) - 1) * bar_width
    plt.xticks(bar_centers, labels)



    # 隐藏边框
    plt.box(False)

    # 显示图例
    plt.legend(data_names, facecolor='#303030', edgecolor='#212121', labelcolor='white')

    # 显示图表
    # plt.show()
    plt.savefig('../images/performance2.png', dpi=300, bbox_inches='tight')

# 示例数据
labels = [
    '0.4 billion',
    '0.6 billion', 
    '0.8 billion', 
    '1.0 billion',
    ]
data_names = ['Swr off & Culling off', 'Swr off & Culling on', 'Swr on & Culling on']
data = [
    [ 109,  84,   71,   54],
    [ 381,  336,  270,  251],
    [ 477,  420,  353,  320],
    ]

# 调用函数生成Nvidia风格的图表
create_nvidia_style_chart(data, data_names, labels, title="Performance of SoftRas", x_label="Triangles on screen", y_label="Framerate", figsize=(15, 6))
