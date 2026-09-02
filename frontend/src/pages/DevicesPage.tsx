import { useEffect, useState } from 'react';
import { ApiClient } from '../api/client';
import type { Device } from '../api/types';
import { DeviceRow } from '../components/display';

export function DevicesPage({ client }: { client: ApiClient }) {
  const [devices, setDevices] = useState<Device[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    client
      .devices()
      .then((d) => {
        if (alive) setDevices(d);
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
        <h1>设备</h1>
        <p className="error-text" role="alert">加载失败：{error}</p>
      </main>
    );
  }
  if (devices === null) {
    return (
      <main className="page" aria-busy="true">
        <h1>设备</h1>
        <p className="empty-hint">加载中…</p>
      </main>
    );
  }
  return (
    <main className="page">
      <h1>设备</h1>
      {devices.length === 0 ? (
        <p className="empty-hint">还没有绑定设备。</p>
      ) : (
        <ul className="device-list" aria-label="设备列表">
          {devices.map((d) => (
            <DeviceRow key={d.device_id} device={d} />
          ))}
        </ul>
      )}
    </main>
  );
}
