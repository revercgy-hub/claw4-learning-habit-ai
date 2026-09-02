import { useEffect, useState } from 'react';
import { ApiClient } from '../api/client';
import type { Dashboard } from '../api/types';
import { DashboardCards } from '../components/display';

export function DashboardPage({ client }: { client: ApiClient }) {
  const [data, setData] = useState<Dashboard | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    setError(null);
    client
      .dashboard()
      .then((d) => {
        if (alive) setData(d);
      })
      .catch((e: Error) => {
        if (alive) setError(e.message);
      });
    return () => {
      alive = false;
    };
  }, [client]);

  if (error) {
    return (
      <main className="page">
        <h1>学习概况</h1>
        <p className="error-text" role="alert">加载失败：{error}</p>
        <button type="button" onClick={() => window.location.reload()}>重试</button>
      </main>
    );
  }
  if (!data) {
    return (
      <main className="page" aria-busy="true">
        <h1>学习概况</h1>
        <p className="empty-hint">加载中…</p>
      </main>
    );
  }
  return (
    <main className="page">
      <h1>学习概况 <span className="date">{data.date}</span></h1>
      <DashboardCards data={data} />
    </main>
  );
}
