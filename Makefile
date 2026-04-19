# CRDL Compiler Makefile
# 提供便捷的构建和安装命令

.PHONY: all build test clean install uninstall help

# 默认目标
all: build

# 构建项目
build:
	@echo "构建 CRDL 编译器..."
	@mkdir -p build
	@cd build && cmake .. && make

# 运行测试
test: build
	@echo "运行测试套件..."
	@cd build && make test

# 清理构建文件
clean:
	@echo "清理构建文件..."
	@rm -rf build

# 安装到系统 (需要 root 权限)
install: build
	@echo "安装到 /usr/local/bin (需要 root 权限)..."
	@cd build && sudo make install

# 卸载 (需要 root 权限)
uninstall:
	@echo "从系统卸载..."
	@sudo rm -f /usr/local/bin/fox-route
	@sudo rm -rf /usr/local/share/fox-route
	@sudo rm -rf /usr/local/share/doc/fox-route
	@echo "卸载完成"

# 构建并测试安装（到临时目录）
test-install: build
	@echo "测试安装到临时目录..."
	@rm -rf /tmp/crdl_install_test
	@cd build && make install DESTDIR=/tmp/crdl_install_test
	@echo "测试安装完成，文件位于 /tmp/crdl_install_test"
	@echo "测试可执行文件:"
	@/tmp/crdl_install_test/usr/local/bin/fox-route --help || true

# 显示帮助信息
help:
	@echo "CRDL Compiler 构建系统"
	@echo ""
	@echo "可用命令:"
	@echo "  build        构建项目"
	@echo "  test         运行测试"
	@echo "  install      安装到系统 (/usr/local/bin)"
	@echo "  uninstall    从系统卸载"
	@echo "  test-install 测试安装到临时目录"
	@echo "  clean        清理构建文件"
	@echo "  help         显示此帮助信息"
	@echo ""
	@echo "示例:"
	@echo "  make build           # 构建项目"
	@echo "  make test            # 运行所有测试"
	@echo "  sudo make install    # 安装到系统"
	@echo "  sudo make uninstall  # 从系统卸载"