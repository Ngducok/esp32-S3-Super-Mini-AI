# Contributing to ESP32-S3 Micro-LLM

<p align="left">
  <b>Language:</b> 
  <a href="CONTRIBUTING.md">English</a> | 
  <a href="CONTRIBUTING_VN.md">Tiếng Việt</a>
</p>

---

Thank you for your interest in contributing to the ESP32-S3 Micro-LLM project. We welcome code contributions, bug reports, performance optimizations, and documentation improvements.

---

## How Can You Contribute?

1. **Bug Reports**: If you find an issue or crash during compilation or inference, please open a GitHub Issue with your hardware details (silicon revision, Flash size), IDF version, and serial monitor backtrace.
2. **Feature Additions**:
   - INT4 or sub-byte weight quantization kernels.
   - Linear attention / State-Space Model architectures (RWKV, Mamba-Micro).
   - Dynamic prompt caching and speculative decoding.
3. **Documentation**: Enhancing explanations, porting instructions, and benchmark verifications on different ESP32-S3 board variants.

---

## Contribution Workflow

### 1. Fork the Repository
Click the **Fork** button at the top right of the GitHub repository to create your own copy.

### 2. Clone and Create a Branch
```bash
git clone https://github.com/YOUR-USERNAME/esp32-S3-Super-Mini-AI.git
cd esp32-S3-Super-Mini-AI
git checkout -b feature/your-feature-name
```

### 3. Coding Guidelines
- **Zero Dynamic Allocations in Inference**: Do not introduce `malloc` / `free` calls inside the token generation hot path. Keep memory footprint static.
- **Dual-Language Documentation**: When adding new features or sub-folders, ensure both English and Vietnamese documentation files are updated.
- **Code Style**: Use idiomatic, modern C++17/C++20 with strongly-typed namespaces (`LLM`, `Web`, `Config`, `Diagnostics`). Keep comments concise and informative.
- **No Icon Clutter**: Maintain professional documentation formatting without emoji clutter.

### 4. Commit and Push
Follow standard semantic commit message conventions:
```bash
git commit -m "feat: add INT4 matrix-vector multiplication kernel"
git push origin feature/your-feature-name
```

### 5. Submit a Pull Request
1. Navigate to your fork on GitHub and click **Compare & pull request**.
2. Provide a clear title and description explaining your changes, the rationale, and the test results on actual ESP32-S3 hardware.
3. Submit the Pull Request for review.
