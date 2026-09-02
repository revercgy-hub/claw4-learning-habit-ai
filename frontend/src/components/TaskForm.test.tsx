import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, expect, it, vi } from 'vitest';
import { TaskForm } from './TaskForm';
import type { Task } from '../api/types';

function taskFixture(): Task {
  return {
    task_id: 'task-1',
    title: '完成练习册 P32',
    subject: 'math',
    estimated_minutes: 25,
    priority: 'high',
    status: 'pending',
    scheduled_date: '2026-01-01',
    version: 1,
  };
}

describe('TaskForm', () => {
  it('renders labelled fields for create mode', () => {
    render(<TaskForm onSubmit={() => undefined} />);
    expect(screen.getByLabelText('科目')).toBeInTheDocument();
    expect(screen.getByLabelText('任务内容')).toBeInTheDocument();
    expect(screen.getByLabelText('预计分钟')).toBeInTheDocument();
    expect(screen.getByLabelText('优先级')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: '创建任务' })).toBeInTheDocument();
  });

  it('shows validation errors and does not submit an empty form', async () => {
    const user = userEvent.setup();
    const onSubmit = vi.fn();
    render(<TaskForm onSubmit={onSubmit} />);
    const title = screen.getByLabelText('任务内容');
    await user.clear(title);
    const minutes = screen.getByLabelText('预计分钟');
    await user.clear(minutes);
    await user.click(screen.getByRole('button', { name: '创建任务' }));
    expect(screen.getAllByRole('alert').length).toBeGreaterThanOrEqual(1);
    expect(onSubmit).not.toHaveBeenCalled();
  });

  it('submits trimmed values on a valid form', async () => {
    const user = userEvent.setup();
    const onSubmit = vi.fn();
    render(<TaskForm onSubmit={onSubmit} />);
    await user.clear(screen.getByLabelText('任务内容'));
    await user.type(screen.getByLabelText('任务内容'), '  背古诗 20 首  ');
    await user.clear(screen.getByLabelText('预计分钟'));
    await user.type(screen.getByLabelText('预计分钟'), '30');
    await user.selectOptions(screen.getByLabelText('科目'), 'chinese');
    await user.selectOptions(screen.getByLabelText('优先级'), 'high');
    await user.click(screen.getByRole('button', { name: '创建任务' }));
    expect(onSubmit).toHaveBeenCalledTimes(1);
    const payload = onSubmit.mock.calls[0][0];
    expect(payload.title).toBe('背古诗 20 首');
    expect(payload.estimated_minutes).toBe(30);
    expect(payload.subject).toBe('chinese');
    expect(payload.priority).toBe('high');
  });

  it('pre-fills values in edit mode and submits an update', async () => {
    const user = userEvent.setup();
    const onSubmit = vi.fn();
    render(<TaskForm initial={taskFixture()} onSubmit={onSubmit} onCancel={() => undefined} />);
    expect(screen.getByLabelText('任务内容')).toHaveValue('完成练习册 P32');
    expect(screen.getByRole('button', { name: '保存修改' })).toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: '保存修改' }));
    expect(onSubmit).toHaveBeenCalledTimes(1);
  });

  it('supports keyboard-only submission via Enter', async () => {
    const user = userEvent.setup();
    const onSubmit = vi.fn();
    render(<TaskForm onSubmit={onSubmit} />);
    const title = screen.getByLabelText('任务内容');
    await user.type(title, '键盘提交任务');
    await user.keyboard('{Enter}');
    expect(onSubmit).toHaveBeenCalledTimes(1);
  });
});
