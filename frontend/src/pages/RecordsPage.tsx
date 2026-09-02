import { useEffect, useState } from 'react';
import { ApiClient } from '../api/client';
import type { StudySession } from '../api/types';
import { RecordRow } from '../components/display';

const CHILD_ID = 'child-1';

export function RecordsPage({ client }: { client: ApiClient }) {
  const [records, setRecords] = useState<StudySession[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    client
      .studySessions(CHILD_ID)
      .then((r) => {
        if (alive) setRecords(r);
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
        <h1>学习记录</h1>
        <p className="error-text" role="alert">加载失败：{error}</p>
      </main>
    );
  }
  if (records === null) {
    return (
      <main className="page" aria-busy="true">
        <h1>学习记录</h1>
        <p className="empty-hint">加载中…</p>
      </main>
    );
  }
  return (
    <main className="page">
      <h1>学习记录</h1>
      {records.length === 0 ? (
        <p className="empty-hint">还没有学习记录。</p>
      ) : (
        <ul className="record-list" aria-label="学习记录列表">
          {records.map((r) => (
            <RecordRow key={r.session_id} record={r} />
          ))}
        </ul>
      )}
    </main>
  );
}
