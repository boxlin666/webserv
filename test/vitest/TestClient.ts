import net from 'net';

export interface HttpResponse {
    status: number;
    headers: Record<string, string>;
    body: Buffer;
    raw: string;
}

export class TestClient {
    constructor(private port: number = 8080, private host: string = '127.0.0.1') { }

    private parseResponse(raw: string): HttpResponse {
        const parts = raw.split('\r\n\r\n');
        const head = parts[0];
        const bodyStr = parts.slice(1).join('\r\n\r\n');

        const lines = head.split('\r\n');
        const statusLine = lines[0] || '';
        const status = parseInt(statusLine.split(' ')[1] || '0');

        const headers: Record<string, string> = {};
        lines.slice(1).forEach(line => {
            const [key, val] = line.split(': ');
            if (key) headers[key.toLowerCase()] = val;
        });

        return { status, headers, body: Buffer.from(bodyStr), raw };
    }
    private async rawRequest(request: string): Promise<string> {
        return new Promise((resolve, reject) => {
            const client = net.createConnection({ port: this.port, host: this.host });
            let data = '';

            // 设置一个短超时，防止服务器真的挂死
            client.setTimeout(2000, () => {
                client.destroy();
                reject(new Error('Socket Timeout'));
            });

            client.on('connect', () => {
                client.write(request);
                // 重要：告诉服务器客户端发送结束，触发服务器的 read 结束
                // 只有在非 Keep-Alive 场景或者你确定不再发数据时用
                client.end();
            });

            client.on('data', (chunk) => data += chunk.toString());

            // 监听 end 事件 (如果服务器主动关闭)
            client.on('end', () => resolve(data));

            // 监听错误
            client.on('error', reject);
        });
    }

    // 核心功能：封装 POST，自动处理 Content-Length 和格式
    async post(path: string, body: string, headers: Record<string, string> = {}) {
        const headerStr = Object.entries({
            'Host': 'localhost',
            'Content-Length': body.length,
            'Connection': 'close',
            ...headers
        }).map(([k, v]) => `${k}: ${v}`).join('\r\n');

        const request = `POST ${path} HTTP/1.1\r\n${headerStr}\r\n\r\n${body}`;
        return this.rawRequest(request);
    }

    // 核心功能：封装 GET
    async get(path: string, headers: Record<string, string> = {}) {
        const request = `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`;

        // 1. 获取原始字符串
        const rawResponse = await this.rawRequest(request);

        // 2. 解析状态码 (例如: HTTP/1.1 200 OK -> 提取 200)
        const statusLine = rawResponse.split('\r\n')[0]; // "HTTP/1.1 200 OK"
        const status = parseInt(statusLine.split(' ')[1]); // 200

        // 3. 必须返回包含 status 的对象
        return { status, raw: rawResponse };
    }

    async sendChunks(chunks: string[], intervalMs: number = 50): Promise<HttpResponse> {
        return new Promise((resolve, reject) => {
            const client = net.createConnection({ port: this.port });
            let data = '';

            client.on('connect', async () => {
                for (const chunk of chunks) {
                    client.write(chunk);
                    await new Promise(r => setTimeout(r, intervalMs));
                }
                // 【关键修复】：所有分片发送完毕，发送 FIN 包，告诉服务器请求结束
                client.end();
            });

            client.on('data', (chunk) => data += chunk.toString());
            client.on('end', () => resolve(this.parseResponse(data)));
            client.on('error', reject);
        });
    }
}