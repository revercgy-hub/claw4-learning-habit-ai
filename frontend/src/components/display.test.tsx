import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import type { Device, StudySession, Task } from '../api/types';
import { DashboardCards, DeviceRow, RecordRow, StatCard } from './display';
import { TaskList } from './TaskList';

function task(over: Partial<Task>): Task {
  return {
    task_id: 't1', title: '任务A', subject: 'math', estimated_minutes: 20,
    priority: 'medium', status: 'ready', scheduled_date: '2026-01-01', version: 1,
    ...over,
  };
}

describe('TaskList', () => {
  it('shows an empty hint when there are no tasks', () => {
    render(<TaskList tasks={[]} />);
    expect(screen.getByText(/今日还没有任务/)).toBeInTheDocument();
  });

  it('renders task rows with status labels', () => {
    render(<TaskList tasks={[task({}), task({ task_id: 't2', title: '任务B', status: 'completed' })]} />);
    expect(screen.getByText('任务A')).toBeInTheDocument();
    expect(screen.getByText('任务B')).toBeInTheDocument();
    expect(screen.getByText('已完成')).toBeInTheDocument();
  });

  it('exposes edit actions only when onEdit is provided', async () => {
    const { rerender } = render(<TaskList tasks={[task({})]} />);
    expect(screen.queryByRole('button', { name: '编辑' })).not.toBeInTheDocument();
    rerender(<TaskList tasks={[task({})]} onEdit={() => undefined} />);
    expect(screen.getAllByRole('button', { name: '编辑' }).length).toBe(1);
  });
});

describe('display components', () => {
  it('StatCard shows label and value', () => {
    render(<StatCard label="专注分钟" value={42} />);
    expect(screen.getByText('42')).toBeInTheDocument();
    expect(screen.getByText('专注分钟')).toBeInTheDocument();
  });

  it('DashboardCards shows a 0% rate and current activity fallback', () => {
    render(
      <DashboardCards
        data={{
          date: '2026-09-02', planned_tasks: 3, completed_tasks: 0,
          completion_rate: 0, focus_minutes: 5, current_activity: null,
        }}
      />,
    );
    expect(screen.getByText('0%')).toBeInTheDocument();
    expect(screen.getByText(/空闲/)).toBeInTheDocument();
  });

  it('RecordRow formats duration and XP', () => {
    const rec: StudySession = {
      session_id: 's1', task_id: 't1', task_title: '数学', status: 'completed',
      completion_type: 'manual', actual_seconds: 605, pause_count: 2,
      pause_seconds: 10, xp: 10, started_at: 1_800_000_000, finished_at: 1_800_000_700,
    };
    render(<RecordRow record={rec} />);
    expect(screen.getByText('10 分 5 秒')).toBeInTheDocument();
    expect(screen.getByText('+10 XP')).toBeInTheDocument();
    expect(screen.getByText('已完成')).toBeInTheDocument();
  });

  it('DeviceRow shows 未知 battery for null and online state', () => {
    const dev: Device = {
      device_id: 'dev-1', model: 'metalio-claw-4', fw_version: '2.0.51',
      online: false, battery_percent: null, last_seen_at: null,
      last_sync_at: null, last_acked_sequence: 0,
    };
    render(<DeviceRow device={dev} />);
    expect(screen.getByText(/电量 未知/)).toBeInTheDocument();
    expect(screen.getByText('离线')).toBeInTheDocument();
    expect(screen.getByText(/从未同步/)).toBeInTheDocument();
  });
});
