import { useCallback, useEffect, useState } from 'react';
import { ApiClient } from '../api/client';
import type { Task } from '../api/types';
import { TaskForm } from '../components/TaskForm';
import { TaskList } from '../components/TaskList';
import type { TaskFormValues } from '../lib/format';

const CHILD_ID = 'child-1'; // host-MVP development-session family

export type TaskPageMode = 'list' | 'create' | 'edit';

export function TasksPage({ client }: { client: ApiClient }) {
  const [tasks, setTasks] = useState<Task[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [mode, setMode] = useState<TaskPageMode>('list');
  const [editing, setEditing] = useState<Task | null>(null);
  const [busy, setBusy] = useState(false);

  const load = useCallback(async () => {
    setError(null);
    try {
      const today = await client.tasksToday(CHILD_ID);
      setTasks(today.tasks);
    } catch (e) {
      setError(e instanceof Error ? e.message : '加载失败');
    }
  }, [client]);

  useEffect(() => {
    void load();
  }, [load]);

  async function handleCreate(v: TaskFormValues) {
    setBusy(true);
    setError(null);
    try {
      await client.createTask(CHILD_ID, {
        subject: v.subject,
        title: v.title,
        estimated_minutes: v.estimated_minutes as number,
        priority: v.priority,
      });
      setMode('list');
      await load();
    } catch (e) {
      setError(e instanceof Error ? e.message : '创建失败');
    } finally {
      setBusy(false);
    }
  }

  async function handleUpdate(v: TaskFormValues) {
    if (!editing) return;
    setBusy(true);
    setError(null);
    try {
      await client.updateTask(CHILD_ID, editing.task_id, {
        subject: v.subject,
        title: v.title,
        estimated_minutes: v.estimated_minutes as number,
        priority: v.priority,
      });
      setEditing(null);
      setMode('list');
      await load();
    } catch (e) {
      setError(e instanceof Error ? e.message : '保存失败');
    } finally {
      setBusy(false);
    }
  }

  function startEdit(t: Task) {
    setEditing(t);
    setMode('edit');
  }

  function cancel() {
    setEditing(null);
    setMode('list');
  }

  return (
    <main className="page">
      <div className="page-head">
        <h1>今日任务</h1>
        {mode === 'list' ? (
          <button type="button" className="primary" onClick={() => setMode('create')}>
            创建任务
          </button>
        ) : null}
      </div>
      {error ? (
        <p className="error-text" role="alert">{error}</p>
      ) : null}
      {mode === 'create' ? (
        <section aria-label="新建任务">
          <TaskForm busy={busy} onSubmit={(v) => void handleCreate(v)} onCancel={cancel} />
        </section>
      ) : null}
      {mode === 'edit' && editing ? (
        <section aria-label="编辑任务">
          <TaskForm
            initial={editing}
            busy={busy}
            onSubmit={(v) => void handleUpdate(v)}
            onCancel={cancel}
          />
        </section>
      ) : null}
      {mode === 'list' ? (
        tasks === null ? (
          <p className="empty-hint" aria-busy="true">加载中…</p>
        ) : (
          <TaskList tasks={tasks} onEdit={startEdit} />
        )
      ) : null}
    </main>
  );
}
