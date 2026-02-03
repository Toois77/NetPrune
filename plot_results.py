import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os

# ==========================================
# 1. 全局样式设置 (PDF + Scalpel Style)
# ==========================================
def set_paper_style():
    try:
        plt.rcParams['font.family'] = 'serif'
        plt.rcParams['font.serif'] = ['Times New Roman']
    except:
        pass
        
    plt.rcParams['font.size'] = 14
    plt.rcParams['axes.labelsize'] = 16
    plt.rcParams['axes.titlesize'] = 16
    plt.rcParams['xtick.labelsize'] = 14
    plt.rcParams['ytick.labelsize'] = 14
    plt.rcParams['legend.fontsize'] = 12
    
    # 线条与刻度
    plt.rcParams['axes.linewidth'] = 1.5
    plt.rcParams['lines.linewidth'] = 2.5
    plt.rcParams['lines.markersize'] = 8
    plt.rcParams['xtick.direction'] = 'in'
    plt.rcParams['ytick.direction'] = 'in'
    
    # PDF 字体嵌入 (保证 Type 1/TrueType 字体，而非 Type 3)
    plt.rcParams['pdf.fonttype'] = 42
    plt.rcParams['ps.fonttype'] = 42

# ==========================================
# 2. 绘制 Exp3: 敏感性分析 (PDF)
# ==========================================
def plot_exp3_pdf():
    csv_path = 'results/exp3-sensitivity.csv'
    
    if os.path.exists(csv_path):
        df = pd.read_csv(csv_path)
    else:
        print("⚠️ 未找到 CSV，使用默认数据...")
        data = {
            'ProcessingDelay(us)': [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            'Legacy_P99_FCT(ms)': [1.61, 1.61, 1.61, 1.0, 1.61, 1.61, 1.61, 1.61, 1.0, 1.61, 1.61],
            'NetPrune_P99_FCT(ms)': [1.0, 1.61, 1.61, 1.61, 1.61, 1.0, 1.61, 1.61, 1.61, 1.61, 1.0]
        }
        df = pd.DataFrame(data)

    fig, ax = plt.subplots(figsize=(8, 5))

    # 绘制 Legacy
    ax.plot(df['ProcessingDelay(us)'], df['Legacy_P99_FCT(ms)'], 
            marker='o', linestyle='--', color='#D62728', 
            label='Legacy (Baseline)', markeredgecolor='black', markeredgewidth=1)

    # 绘制 Scalpel (原 NetPrune)
    ax.plot(df['ProcessingDelay(us)'], df['NetPrune_P99_FCT(ms)'], 
            marker='s', linestyle='-', color='#1F77B4', 
            label='Scalpel (Ours)', markeredgecolor='black', markeredgewidth=1) # 改名

    ax.set_xlabel(r'Switch Processing Delay ($\mu$s)')
    ax.set_ylabel('P99 Flow Completion Time (ms)')
    
    # 修正 Y 轴范围，防止截断
    ax.set_ylim(0, 3.0) 
    ax.set_xlim(0, 100)
    
    ax.grid(True, linestyle=':', color='gray', alpha=0.5)
    
    # 美化边框
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    ax.legend(frameon=False, loc='upper left') 

    # 保存为 PDF
    outfile = 'results/exp3_sensitivity_paper.pdf'
    plt.savefig(outfile, format='pdf', bbox_inches='tight')
    print(f"✅ Exp3 Chart Saved: {outfile}")

# ==========================================
# 3. 绘制 Exp5: 公平性 (PDF)
# ==========================================
def plot_exp5_pdf():
    # 数据 (根据你的实验结果硬编码或读取)
    # 改名: NetPrune Flow -> Scalpel Flow
    labels = ['TCP Flow\n(Legacy)', 'Scalpel Flow\n(Ours)'] 
    bandwidths = [7.27, 2.67] 
    
    fig, ax = plt.subplots(figsize=(6, 5))
    
    # 绘制柱状图 (保留纹理 hatch)
    bars = ax.bar(labels, bandwidths, 
                  color=['white', 'white'], 
                  edgecolor='black',       
                  linewidth=2,
                  hatch=['//', '..'],       
                  width=0.5)
    
    # 填充颜色 (半透明)
    ax.bar(labels, bandwidths, color=['#2CA02C', '#1F77B4'], alpha=0.3, width=0.5)

    # 添加数值标签
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height + 0.2,
                f'{height:.2f} G',
                ha='center', va='bottom', fontsize=14, fontweight='bold')

    # 添加参考线
    ax.axhline(y=10, color='gray', linestyle='--', linewidth=1.5, label='Link Capacity (10G)')
    
    ax.set_ylabel('Throughput (Gbps)')
    ax.set_ylim(0, 11)
    
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.legend(frameon=False, loc='upper right')

    # 保存为 PDF
    outfile = 'results/exp5_fairness_paper.pdf'
    plt.savefig(outfile, format='pdf', bbox_inches='tight')
    print(f"✅ Exp5 Chart Saved: {outfile}")

if __name__ == "__main__":
    if not os.path.exists('results'):
        os.makedirs('results')
        
    set_paper_style()
    print("🚀 Generating Scalpel (Exp3 & Exp5) Charts (PDF Only)...")
    
    plot_exp3_pdf()
    plot_exp5_pdf()
    
    print("\n🎉 All charts updated with 'Scalpel' name!")