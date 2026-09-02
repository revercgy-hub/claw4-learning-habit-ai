import { useState } from 'react';
import { ApiClient } from '../api/client';

export interface LoginProps {
  client: ApiClient;
  onLoggedIn: (token: string) => void;
  message?: string;
}

export function Login({ client, onLoggedIn, message }: LoginProps) {
  const [token, setToken] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const t = token.trim();
    if (!t) {
      setError('请输入家长会话令牌');
      return;
    }
    setBusy(true);
    setError(null);
    try {
      client.setToken(t);
      const me = await client.me();
      if (!me.child_ids.length) {
        throw new Error('该会话没有可管理的孩子');
      }
      onLoggedIn(t);
    } catch (err) {
      setError(err instanceof Error ? err.message : '登录失败');
      client.setToken('');
    } finally {
      setBusy(false);
    }
  }

  return (
    <main className="login">
      <h1>学习习惯家长端</h1>
      <p className="hint">主机 MVP 开发会话：请输入家长会话令牌（如 mock-parent-token-1）。</p>
      {message ? <p className="hint">{message}</p> : null}
      <form onSubmit={submit} noValidate aria-label="登录表单">
        <div className="field">
          <label htmlFor="lg-token">家长会话令牌</label>
          <input
            id="lg-token"
            type="password"
            value={token}
            onChange={(e) => setToken(e.target.value)}
            autoComplete="off"
          />
        </div>
        {error ? (
          <p className="error-text" role="alert">{error}</p>
        ) : null}
        <button type="submit" disabled={busy} className="primary">
          {busy ? '验证中…' : '进入'}
        </button>
      </form>
    </main>
  );
}
