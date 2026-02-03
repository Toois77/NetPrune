import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os

# ==========================================
# 全局样式设置 (Paper Quality)
# ==========================================
def set_paper_style():
    try:
        plt.rcParams['font.family'] = 'serif'
        plt.rcParams['font.serif'] = ['Times New Roman']
    except:
        pass
        
    # 字体大小适配单栏/双栏论文
    plt.rcParams['font.size'] = 14
    plt.rcParams['axes.labelsize'] = 16
    plt.rcParams['axes.titlesize'] = 16
    plt.rcParams['xtick.labelsize'] = 14
    plt.rcParams['ytick.labelsize'] = 14
    plt.rcParams['legend.fontsize'] = 12
    
    # 线条与刻度
    plt.rcParams['axes.linewidth'] = 1.5
    plt.rcParams['lines.linewidth'] = 2.5
    plt.rcParams['xtick.direction'] = 'in'
    plt.rcParams['ytick.direction'] = 'in'
    
    # 确保 PDF 字体可编辑 (Type 42)
    plt.rcParams['pdf.fonttype'] = 42 
    plt.rcParams['ps.fonttype'] = 42

# ==========================================
# Figure 3: Latency CDF (Scalpel vs Legacy)
# ==========================================
def plot_supp1():
    try:
        df = pd.read_csv('results/supp1-latency-cdf-FIXED.csv')
        fig, ax = plt.subplots(figsize=(8, 5))
        
        # 定义样式映射
        styles = {
            'Legacy':   {'ls': '--', 'color': '#D62728', 'lw': 2.5, 'label': 'Legacy (Tail Drop)'},
            'RED':      {'ls': ':',  'color': '#FF7F0E', 'lw': 2.5, 'label': 'RED'},
            'NetPrune': {'ls': '-',  'color': '#1F77B4', 'lw': 3.5, 'label': 'Scalpel (Ours)'} # 改名 Scalpel
        }

        # 指定画图顺序
        order = ['Legacy', 'RED', 'NetPrune']
        
        for scheme in order:
            if scheme not in df['Scheme'].unique(): continue
            
            s = styles[scheme]
            # 获取数据并排序
            data = sorted(df[df['Scheme']==scheme]['Latency_ms'])
            
            if len(data) == 0: continue
            
            # 计算 CDF
            y = np.arange(1, len(data)+1) / len(data)
            
            ax.plot(data, y, label=s['label'], linewidth=s['lw'], linestyle=s['ls'], color=s['color'])
        
        ax.set_xlabel('Latency (ms)')
        ax.set_ylabel('CDF')
        ax.grid(True, linestyle=':', color='gray', alpha=0.5)
        ax.legend(frameon=False, loc='lower right')
        
        # 设置坐标轴范围 (根据数据自动调整，或者手动微调)
        ax.set_ylim(0, 1.05)
        ax.set_xlim(left=0)
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)

        outfile = 'results/fig3_latency_cdf.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Figure 3 Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Fig 3: {e}")

# ==========================================
# Figure 2: Queue Dynamics (Scalpel 锯齿波)
# ==========================================
def plot_supp2():
    try:
        df = pd.read_csv('results/supp2-queue-depth-FIXED.csv')
        fig, ax = plt.subplots(figsize=(10, 4))
        
        # 分离数据
        leg = df[df['Scheme']=='Legacy']
        np_ = df[df['Scheme']=='NetPrune']
        
        # 1. 绘制 Scalpel (蓝色，实线，放在下层)
        # 注意：这里从 CSV 读的是 NetPrune，但 Label 写 Scalpel
        ax.plot(np_['Time_s'], np_['QueueDepth_packets'], 
                color='#1F77B4', alpha=0.9, linewidth=1.5, 
                label='Scalpel (Ours)', zorder=2)
        
        # 2. 绘制 Legacy (红色，虚线，放在上层，防止完全遮挡)
        ax.plot(leg['Time_s'], leg['QueueDepth_packets'], 
                color='#D62728', alpha=0.8, linewidth=2.0, linestyle='--',
                label='Legacy', zorder=10)
        
        # 3. 参考线
        ax.axhline(100, color='gray', linestyle=':', linewidth=1.5, label='Buffer Limit')
        ax.axhline(20, color='#1F77B4', linestyle=':', linewidth=1, alpha=0.6, label='Pruning Threshold')
        
        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Queue Depth (pkts)')
        
        # 截取稳态区间 (1.0s - 4.0s)，去掉前后的启动/结束阶段
        ax.set_xlim(1.0, 4.0)
        ax.set_ylim(0, 110)
        
        # 图例
        ax.legend(frameon=False, loc='upper right', ncol=4, fontsize=11)
        ax.grid(True, linestyle=':', color='gray', alpha=0.5)
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        outfile = 'results/fig2_queue_dynamics.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Figure 2 Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Fig 2: {e}")

# ==========================================
# Figure 5: Bandwidth Breakdown (Scalpel 极致剪枝)
# ==========================================
def plot_supp3():
    try:
        df = pd.read_csv('results/supp3-bandwidth-breakdown-FIXED.csv')
        schemes_raw = ['Legacy', 'NetPrune'] # 指定顺序
        
        # 颜色和纹理配置
        cats_config = {
            'VIP Data':           {'color': '#2CA02C', 'hatch': '//', 'label': 'VIP Data (Effective)'},
            'Valid Drafts':       {'color': '#1F77B4', 'hatch': '\\\\','label': 'Valid Drafts (Effective)'},
            'Pruning Overhead':   {'color': '#FF7F0E', 'hatch': '..', 'label': 'Pruning Overhead'},
            'Dead Draft Payload': {'color': '#D62728', 'hatch': '**', 'label': 'Dead Payload (Waste)'}
        }
        cats_order = ['VIP Data', 'Valid Drafts', 'Pruning Overhead', 'Dead Draft Payload']
        
        fig, ax = plt.subplots(figsize=(6, 6))
        
        bottom = np.zeros(len(schemes_raw))
        x = np.arange(len(schemes_raw))
        width = 0.5
        
        # 堆叠柱状图
        for cat in cats_order:
            if cat not in cats_config: continue
            
            style = cats_config[cat]
            vals = []
            
            for s in schemes_raw:
                # 从 DataFrame 查找对应的值 (注意列名是 Bytes_MB)
                row = df[(df['Scheme']==s) & (df['Category']==cat)]
                if not row.empty:
                    val = row['Bytes_MB'].values[0]
                    vals.append(val)
                else:
                    vals.append(0)
            
            # 绘制
            ax.bar(x, vals, width, bottom=bottom, label=style['label'],
                   color='white', edgecolor='black', linewidth=1, hatch=style['hatch'])
            ax.bar(x, vals, width, bottom=bottom, color=style['color'], alpha=0.6) 
            bottom += np.array(vals)
            
        ax.set_ylabel('Bandwidth Consumption (MB)')
        ax.set_xticks(x)
        
        # 💡 关键：替换 X 轴标签
        display_labels = ['Legacy', 'Scalpel']
        ax.set_xticklabels(display_labels)
        
        # 图例
        handles, labels = ax.get_legend_handles_labels()
        # 反转图例顺序，让 VIP 在最上面 (可选)
        # ax.legend(handles[::-1], labels[::-1], frameon=False, bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.legend(frameon=False, bbox_to_anchor=(0.5, 1.15), loc='center', ncol=2, fontsize=10)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        outfile = 'results/fig5_bandwidth.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Figure 5 Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Fig 5: {e}")

if __name__ == '__main__':
    if not os.path.exists('results'):
        os.makedirs('results')
    
    set_paper_style()
    print("🚀 Generating Final Scalpel Paper Charts (PDF)...")
    
    plot_supp1()
    plot_supp2()
    plot_supp3()
    
    print("\n🎉 All charts generated! Ready for APNet submission.")