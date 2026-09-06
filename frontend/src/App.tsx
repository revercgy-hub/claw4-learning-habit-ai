import { useMemo, useState } from 'react';
import { ApiClient } from './api/client';
import { Login } from './components/Login';
import { DashboardPage } from './pages/DashboardPage';
import { DevicesPage } from './pages/DevicesPage';
import { RecordsPage } from './pages/RecordsPage';
import { TasksPage } from './pages/TasksPage';

export type Tab = 'dashboard' | 'tasks' | 'records' | 'devices';

const TABS: Array<{ id: Tab; label: string }> = [
  { id: 'dashboard', label: '概况' },
  { id: 'tasks', label: '今日任务' },
  { id: 'records', label: '学习记录' },
  { id: 'devices', label: '设备' },
];

export function App() {
  const [token, setToken] = useState<string>(() => sessionStorage.getItem('claw4_parent_token') ?? '');
  const [client] = useState(() => new ApiClient({ token }));
  const [tab, setTab] = useState<Tab>('dashboard');

  const authed = useMemo(() => token.length > 0, [token]);

  function handleLoggedIn(t: string) {
    sessionStorage.setItem('claw4_parent_token', t);
    setToken(t);
    setTab('dashboard');
  }

  function logout() {
    sessionStorage.removeItem('claw4_parent_token');
    client.setToken('');
    setToken('');
  }

  if (!authed) {
    return <Login client={client} onLoggedIn={handleLoggedIn} />;
  }

  return (
    <div className="shell">
      <header className="topbar">
        <span className="brand">学习习惯家长端</span>
        <nav aria-label="主导航">
          {TABS.map((t) => (
            <button
              key={t.id}
              type="button"
              className={tab === t.id ? 'tab active' : 'tab'}
              aria-current={tab === t.id ? 'page' : undefined}
              onClick={() => setTab(t.id)}
            >
              {t.label}
            </button>
          ))}
        </nav>
        <button type="button" className="link" onClick={logout}>
          退出
        </button>
      </header>
      {tab === 'dashboard' ? <DashboardPage client={client} /> : null}
      {tab === 'tasks' ? <TasksPage client={client} /> : null}
      {tab === 'records' ? <RecordsPage client={client} /> : null}
      {tab === 'devices' ? <DevicesPage client={client} /> : null}
    </div>
  );
}
