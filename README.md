## Background

此项目，是为了学习bitcoin的原理及实现细节，并实现了GPU加速bitcoin的双hash计算。

## 功能概述

### 已实现的功能

- Block的IBO(Headers-First)
- Block接收
- Block校验
- TX接收
- TX校验-
- 挖矿(CPU/GPU)
- Block打包
- 发布新Block
- 支持隔离见证
- P2P协议: VERSION VERACK INV GETDATA PING TX HEADERS BLOCK

### 未实现的功能

- Bloom Filter
- 隔离见证 Coinbase Commitment的校验
- 区块时间校验条件: 区块时间 > 过去11个区块的中位时间 < 未来2小时

### 复用bitcoin部分代码

- 序列化
- Script
- VM
- Key管理

## 关于secp256k1

本项目使用的椭圆加密算法实现是比特币使用的secp256k1的静态库
此库的编译选项，可参考本项目中secp256k1内的ru_configure.sh。

## 特殊说明

最长链接收测试过了，可以正常运行。但孤块，分叉块处理，代码目前还没有正式测试。

## GPU 双hash计算(GPU挖矿)

### 前言

目前hash算力，CPU和GPU是运算速度应该是跑不过专门的硬件的。

### GPU计算简述

GPU编程，是将庞大的计算任务，分割成独立小任务后，交给GPU中动辄数 百上千甚至上万的处理单元，并行处理。比如要计算2000个 8 \_ 8 ,如果是cpu处理的话是要循 环2000次，但交给一个有2000个处理单元的GPU计算，每个处理单元计算一个8 \* 8，则只需要一次即可全部计算完成。

### sha256(挖矿算法)

原理: https://blog.csdn.net/u011583927/article/details/80905740/
此项目的sha256算法，是基于Bitcoin Core的sha256修改实现。

### bitcoin 挖矿

1. sha256的规则是对要hash的数据体以512bit分割为一个块，并以前一个块为基础做64次逻辑计算。
2. 比特币挖矿是指对区块头做sha256 得到hash_1，然后再次对hash_1做sha256后得到hash_2 这个hash_2即是最终的区块hash，然后比对这个hash_2 如果<=全网难度，则挖矿成功。

### 实现

因只是用于比特币挖矿的hash(数据体长度固定)，而不是通用的hash(数据体长度不固定只能循环处理) 所以可以将区块头数据hash的指令全部展开为顺序执行，然后交给每个GPU的处理单元 这样，假如GPU有2000个处理单元，那么交给GPU一轮计算就可得到2000个hash结果。

### 实现逻辑

```
流程:
     1. CPU端, 第一次补位: 是将比特币区块头数据补齐，如下:
         区块头640bit(80byte) + 补位384bit(48byte) = 1024bit(512bit*2) = 128byte
     2. 上述补位后的128byte数据交给GPU的每个处理单元，处理单元内的流程如下:
         1. 替换nonce值，如下:
             nonce在比特币头数据第76byte处，长度4个byte，将其修改为新的nonce值。
         2. 将128byte分割为两个512bit的块，并按顺序对每个块执行sha256的64个逻辑运算得到结果hash_1。
         3. 将hash_1补位，如下:
             hash_1 32byte(256bit)  + 补位32byte = 512bit = 64byte
         4. 再次对补位后的hash_1做sha256运算，得到结果hash_2，此hash_2即是最终block hash.
```

### 测试结果

```
2.5 GHz 四核Intel Core i7
16 GB 1600 MHz DDR3
Intel Iris Pro 1536 MB

测试hash次数 20,480,000 次
CPU 消耗时间: 115.809秒
GPU 消耗时间: 11.477 秒
```

### 相关文件

btc_sha256_gpu.h, btc_sha256_gpu.cpp, btc_sha256_gpu.cl
