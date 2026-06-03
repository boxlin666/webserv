import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true, // 允许直接使用 test, expect 等全局变量
    environment: 'node',
  },
});