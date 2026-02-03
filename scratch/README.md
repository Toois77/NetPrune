# NetPrune NS-3 仿真实验指南

## 项目概述

本项目实现了基于NS-3的NetPrune协议仿真实验，包含5个核心实验：
1. **实验一**：拓扑与流量生成
2. **实验二**：核心性能对比 (HoL阻塞)
3. **实验三**：敏感性分析 (交换机处理延迟)
4. **实验四**：安全与校验 (位翻转)
5. **实验五**：公平性测试

## 环境要求

- **NS-3**: 版本 3.35 或更高
- **操作系统**: Ubuntu 20.04/22.04 或其他Linux发行版
- **编译器**: g++ 7.0+ (支持C++17)
- **依赖**: 
  - Python 3.x (用于分析脚本)
  - matplotlib, numpy (用于画图)

## 安装步骤

### 1. 安装NS-3

```bash
# 下载NS-3
cd ~
wget https://www.nsnam.org/releases/ns-allinone-3.35.tar.bz2
tar xjf ns-allinone-3.35.tar.bz2
cd ns-allinone-3.35/ns-3.35

# 配置和编译
./waf configure --enable-examples --enable-tests
./waf build
```

### 2. 安装Python依赖

```bash
pip3 install matplotlib numpy pandas
```

### 3. 部署NetPrune仿真代码

```bash
# 将本项目复制到NS-3的scratch目录
cp -r netprune-simulation ~/ns-allinone-3.35/ns-3.35/scratch/

# 或者创建符号链接
ln -s $(pwd) ~/ns-allinone-3.35/ns-3.35/scratch/netprune-simulation
```

## 编译

```bash
cd ~/ns-allinone-3.35/ns-3.35
./waf configure --enable-examples
./waf build
```

## 运行实验

### 实验一：拓扑与流量生成

验证拓扑和流量模型是否正确：

```bash
./waf --run "netprune-exp1 --duration=10 --bandwidth=10Gbps --rtt=100us --bufferSize=100KB"
```

**参数说明**:
- `--duration`: 仿真时长(秒)
- `--bandwidth`: 瓶颈带宽
- `--rtt`: 往返时延
- `--bufferSize`: 交换机缓存大小

**输出**: 
- 流量统计文件: `results/exp1-traffic-stats.txt`
- PCAP文件: `results/exp1-*.pcap`

---

### 实验二：核心性能对比

对比Legacy UDP vs NetPrune性能：

```bash
# Baseline (Legacy UDP)
./waf --run "netprune-exp2 --mode=legacy --flows=10 --bandwidth=10Gbps"

# NetPrune
./waf --run "netprune-exp2 --mode=netprune --flows=10 --bandwidth=10Gbps"

# 生成对比图
python3 scripts/plot_exp2.py
```

**关键指标**:
- P99 Flow Completion Time (FCT)
- Goodput (有效吞吐量)
- Buffer Occupancy (队列占用率)

**输出**:
- `results/exp2-legacy-fct.txt`
- `results/exp2-netprune-fct.txt`
- `results/exp2-comparison.png` (对比图)

---

### 实验三：敏感性分析

测试不同交换机处理延迟下的性能：

```bash
# 自动扫描不同延迟值
./waf --run "netprune-exp3 --delayStart=0 --delayEnd=100 --delayStep=10"

# 生成延迟vs性能曲线图
python3 scripts/plot_exp3.py
```

**测试延迟范围**:
- 0µs: 理想硬件 (Tofino P4)
- 1-10µs: FPGA实现
- 50-100µs: 控制平面介入

**输出**:
- `results/exp3-sensitivity.csv`
- `results/exp3-latency-curve.png`

---

### 实验四：安全与校验

测试位错误对NetPrune的影响：

```bash
# 不同的位错误率
./waf --run "netprune-exp4 --ber=1e-5"  # BER = 10^-5
./waf --run "netprune-exp4 --ber=1e-4"  # BER = 10^-4 (压力测试)

# 分析误判率
python3 scripts/analyze_exp4.py
```

**验证指标**:
- False Positives (误判率): 正常包被错误丢弃
- False Negatives (漏判率): Prune信号被忽略

**输出**:
- `results/exp4-error-analysis.txt`
- `results/exp4-safety-report.pdf`

---

### 实验五：公平性测试

测试NetPrune与TCP的共存：

```bash
./waf --run "netprune-exp5 --tcpFlows=1 --netpruneFlows=1 --duration=30"

# 生成吞吐量对比
python3 scripts/plot_exp5.py
```

**测试场景**:
- 1条NetPrune流 (UDP)
- 1条背景TCP流 (TCP CUBIC)
- 共享10Gbps瓶颈带宽

**输出**:
- `results/exp5-fairness.png` (吞吐量对比柱状图)
- `results/exp5-tcp-throughput.txt`
- `results/exp5-netprune-throughput.txt`

---

## 一键运行所有实验

```bash
bash scripts/run_all_experiments.sh
```

这会依次运行所有5个实验并生成所有图表。

---

## 结果分析

所有实验结果保存在 `results/` 目录：

```
results/
├── exp1-traffic-stats.txt          # 实验1：流量统计
├── exp2-comparison.png             # 实验2：性能对比图
├── exp2-buffer-occupancy.png       # 实验2：队列占用率
├── exp3-latency-curve.png          # 实验3：延迟敏感性曲线
├── exp4-error-analysis.txt         # 实验4：错误分析
├── exp5-fairness.png               # 实验5：公平性对比
└── summary-report.pdf              # 总结报告
```

### 预期实验结果

**实验二 (性能对比)**:
- NetPrune P99 FCT 应降低 **30-50%**
- Goodput 提升 **20-40%**
- Buffer Occupancy 呈现"锯齿状"下降

**实验三 (敏感性)**:
- 即使处理延迟100µs，NetPrune仍优于Baseline
- P99延迟缓慢上升，但始终低于Legacy

**实验四 (安全性)**:
- False Positive 率应为 **0%**
- False Negative 率 < 0.1% (可接受)

**实验五 (公平性)**:
- TCP吞吐量无明显下降
- NetPrune Goodput上升，Raw Throughput下降

---

## 故障排查

### 1. 编译错误

```bash
# 清理后重新编译
./waf clean
./waf configure --enable-examples
./waf build
```

### 2. 运行时错误

检查NS-3版本：
```bash
./waf --version
```

### 3. 缺少依赖

```bash
# Ubuntu/Debian
sudo apt-get install g++ python3 python3-dev python3-pip
pip3 install -r requirements.txt
```

### 4. 结果文件未生成

确保 `results/` 目录存在且有写权限：
```bash
mkdir -p results
chmod 755 results
```

---

## 自定义实验参数

每个实验都支持命令行参数调整。查看帮助：

```bash
./waf --run "netprune-exp1 --help"
./waf --run "netprune-exp2 --help"
# ... 等等
```

常用参数：
- `--bandwidth`: 带宽 (例如 10Gbps, 40Gbps)
- `--rtt`: RTT延迟 (例如 100us, 200us)
- `--bufferSize`: 缓存大小 (例如 100KB, 1MB)
- `--flows`: 流数量
- `--duration`: 仿真时长(秒)

---

## 论文图表生成

生成论文级别的图表：

```bash
python3 scripts/generate_paper_figures.py
```

输出位于 `results/paper-figures/`：
- 高分辨率PNG (300 DPI)
- 矢量图PDF
- LaTeX兼容的EPS格式

---

## 引用

如果使用本代码，请引用：

```bibtex
@inproceedings{netprune2024,
  title={NetPrune: Eliminating Head-of-Line Blocking in Speculative Inference},
  author={Your Name},
  booktitle={Conference},
  year={2024}
}
```

---

## 联系与支持

- 问题反馈: GitHub Issues
- 邮件: your-email@example.com

---

## 许可证

MIT License
