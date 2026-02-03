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
    plt.rcParams['font.size'] = 14
    plt.rcParams['axes.labelsize'] = 16
    plt.rcParams['axes.titlesize'] = 16
    plt.rcParams['legend.fontsize'] = 12
    plt.rcParams['lines.linewidth'] = 2
    plt.rcParams['axes.linewidth'] = 1.5
    # PDF 字体嵌入设置
    plt.rcParams['pdf.fonttype'] = 42 
    plt.rcParams['ps.fonttype'] = 42

# ==========================================
# Supp 1: Latency CDF (Scalpel vs Legacy)
# ==========================================
def plot_supp1():
    try:
        df = pd.read_csv('results/supp1-latency-cdf.csv')
        fig, ax = plt.subplots(figsize=(8, 5))
        
        # 样式映射：CSV中的名字 -> 画图样式
        # 重点：NetPrune 的 label 改为 "Scalpel (Ours)"
        styles = {
            'Legacy':   {'ls': '--', 'color': '#D62728', 'lw': 2, 'label': 'Legacy'},
            'ECN':      {'ls': ':',  'color': '#FF7F0E', 'lw': 2, 'label': 'ECN/RED'},
            'PFC':      {'ls': '-.', 'color': '#9467BD', 'lw': 2, 'label': 'PFC'},
            'RTS':      {'ls': (0, (3, 1, 1, 1)), 'color': '#17BECF', 'lw': 2, 'label': 'RTS'},
            'NetPrune': {'ls': '-',  'color': '#1F77B4', 'lw': 3, 'label': 'Scalpel (Ours)'} 
        }

        for scheme in df['Scheme'].unique():
            # 获取样式，如果没找到就用默认
            s = styles.get(scheme, {'ls': '-', 'color': 'gray', 'lw': 1, 'label': scheme})
            
            # 获取数据并排序
            data = sorted(df[df['Scheme']==scheme]['Latency(ms)'])
            y = np.arange(1, len(data)+1) / len(data)
            
            # 画图
            ax.plot(data, y, label=s['label'], linewidth=s['lw'], linestyle=s['ls'], color=s['color'])
        
        ax.set_xlabel('Latency (ms)')
        ax.set_ylabel('CDF')
        ax.grid(True, linestyle=':', color='gray', alpha=0.5)
        ax.legend(frameon=False, loc='lower right')
        ax.set_xlim(left=0)
        ax.set_ylim(0, 1.05)
        
        # 美化边框
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)

        # 只保存 PDF
        outfile = 'results/supp1_latency_cdf.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Supp1 Chart Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Supp1: {e}")

# ==========================================
# Supp 2: Queue Dynamics (Scalpel 锯齿波)
# ==========================================
def plot_supp2():
    try:
        df = pd.read_csv('results/supp2-queue-timeseries.csv')
        fig, ax = plt.subplots(figsize=(10, 4))
        
        # 筛选数据 (CSV 里还是叫 Legacy 和 NetPrune)
        leg = df[df['Mode']=='Legacy']
        np_ = df[df['Mode']=='NetPrune']
        
        # 1. 绘制 Scalpel (原 NetPrune) - 底层，蓝色
        ax.plot(np_['Time(s)'], np_['QueueDepth(packets)'], 
                color='#1F77B4', alpha=0.8, linewidth=1.5, 
                label='Scalpel (Ours)', zorder=2) # 改名
        
        # 2. 绘制 Legacy - 顶层，红色，虚线，防止遮挡
        ax.plot(leg['Time(s)'], leg['QueueDepth(packets)'], 
                color='#D62728', alpha=1.0, linewidth=2.5, linestyle='--',
                label='Legacy', zorder=10)
        
        # 3. 参考线
        ax.axhline(100, color='gray', linestyle=':', linewidth=1.5, label='Buffer Limit')
        # 改名为 Scalpel Threshold
        ax.axhline(20, color='#1F77B4', linestyle=':', linewidth=1, alpha=0.6, label='Scalpel Threshold')
        
        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Queue Depth (pkts)')
        
        # 动态调整 X 轴范围，根据数据的最大时间
        max_time = df['Time(s)'].max()
        # 聚焦在后半段稳定期 (假设前半段是预热)
        start_view = max(0.0, max_time - 0.6) 
        ax.set_xlim(start_view, max_time)
        ax.set_ylim(0, 110)
        
        # 图例
        ax.legend(frameon=False, loc='upper right', ncol=4, fontsize=11)
        ax.grid(True, linestyle=':', color='gray', alpha=0.5)
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        outfile = 'results/supp2_queue_dynamics.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Supp2 Chart Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Supp2: {e}")

# ==========================================
# Supp 3: Bandwidth Breakdown (Scalpel 极致剪枝)
# ==========================================
def plot_supp3():
    try:
        df = pd.read_csv('results/supp3-bandwidth-breakdown.csv')
        # 获取原始方案名 (Legacy, NetPrune)
        schemes_raw = df['Scheme'].unique()
        
        cats_config = {
            'VIP Data':        {'color': '#2CA02C', 'hatch': '//', 'label': 'VIP Data (Effective)'},
            'Valid Drafts':    {'color': '#1F77B4', 'hatch': '\\\\','label': 'Valid Drafts (Effective)'},
            'Header Overhead': {'color': '#FF7F0E', 'hatch': '..', 'label': 'Pruning Overhead'},
            'Retransmits':     {'color': '#9467BD', 'hatch': 'xx', 'label': 'Retransmissions (Waste)'},
            'Dead Payload':    {'color': '#D62728', 'hatch': '**', 'label': 'Dead Payload (Waste)'}
        }
        cats_order = ['VIP Data', 'Valid Drafts', 'Header Overhead', 'Retransmits', 'Dead Payload']
        
        fig, ax = plt.subplots(figsize=(7, 6))
        bottom = np.zeros(len(schemes_raw))
        x = np.arange(len(schemes_raw))
        width = 0.5
        
        for cat in cats_order:
            if cat not in cats_config: continue
            style = cats_config[cat]
            vals = []
            for s in schemes_raw:
                v = df[(df['Scheme']==s) & (df['Category']==cat)]['Bytes(MB)'].values
                vals.append(v[0] if len(v)>0 else 0)
            
            # 画柱状图
            ax.bar(x, vals, width, bottom=bottom, label=style['label'],
                   color='white', edgecolor='black', linewidth=1, hatch=style['hatch'])
            ax.bar(x, vals, width, bottom=bottom, color=style['color'], alpha=0.6) 
            bottom += vals
            
        ax.set_ylabel('Bandwidth Consumption (MB)')
        ax.set_xticks(x)
        
        # 💡 关键修改：把 X 轴标签里的 "NetPrune" 替换为 "Scalpel"
        display_labels = [s if s != 'NetPrune' else 'Scalpel' for s in schemes_raw]
        ax.set_xticklabels(display_labels)
        
        ax.legend(frameon=False, bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        outfile = 'results/supp3_goodput.pdf'
        plt.savefig(outfile, format='pdf', bbox_inches='tight')
        print(f"✅ Supp3 Chart Saved: {outfile}")
        plt.close()
    except Exception as e:
        print(f"⚠️ Skip Supp3: {e}")

if __name__ == '__main__':
    if not os.path.exists('results'):
        os.makedirs('results')
    
    set_paper_style()
    print("🚀 Generating Scalpel (APNet) Evaluation Charts (PDF Only)...")
    
    plot_supp1()
    plot_supp2()
    plot_supp3()
    
    print("\n🎉 All charts updated with 'Scalpel' name!")