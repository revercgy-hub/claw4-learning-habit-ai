import { describe, expect, it } from 'vitest';
import {
  batteryLabel,
  formatDuration,
  hasErrors,
  validateTaskForm,
} from './format';

describe('formatDuration', () => {
  it('renders zero as 0 秒', () => {
    expect(formatDuration(0)).toBe('0 秒');
    expect(formatDuration(-5)).toBe('0 秒');
  });
  it('renders pure seconds', () => {
    expect(formatDuration(45)).toBe('45 秒');
  });
  it('renders minutes and seconds', () => {
    expect(formatDuration(125)).toBe('2 分 5 秒');
  });
  it('renders whole minutes without seconds', () => {
    expect(formatDuration(600)).toBe('10 分');
  });
  it('floors fractional input', () => {
    expect(formatDuration(61.9)).toBe('1 分 1 秒');
  });
});

describe('batteryLabel', () => {
  it('shows 未知 for null/undefined (never fabricated)', () => {
    expect(batteryLabel(null)).toBe('未知');
    expect(batteryLabel(undefined)).toBe('未知');
  });
  it('shows a real percentage when present', () => {
    expect(batteryLabel(72)).toBe('72%');
    expect(batteryLabel(0)).toBe('0%');
  });
});

describe('validateTaskForm', () => {
  it('requires a title', () => {
    const errs = validateTaskForm({
      title: '   ',
      subject: 'math',
      estimated_minutes: 20,
      priority: 'medium',
    });
    expect(errs.title).toBeDefined();
    expect(hasErrors(errs)).toBe(true);
  });
  it('limits title length', () => {
    const errs = validateTaskForm({
      title: 'x'.repeat(201),
      subject: 'math',
      estimated_minutes: 20,
      priority: 'medium',
    });
    expect(errs.title).toBeDefined();
  });
  it('requires minutes when empty', () => {
    const errs = validateTaskForm({
      title: 'ok',
      subject: 'math',
      estimated_minutes: '',
      priority: 'medium',
    });
    expect(errs.estimated_minutes).toBeDefined();
  });
  it('rejects out-of-range minutes', () => {
    const low = validateTaskForm({
      title: 'ok', subject: 'math', estimated_minutes: 0, priority: 'medium',
    });
    const high = validateTaskForm({
      title: 'ok', subject: 'math', estimated_minutes: 601, priority: 'medium',
    });
    const frac = validateTaskForm({
      title: 'ok', subject: 'math', estimated_minutes: 2.5, priority: 'medium',
    });
    expect(low.estimated_minutes).toBeDefined();
    expect(high.estimated_minutes).toBeDefined();
    expect(frac.estimated_minutes).toBeDefined();
  });
  it('accepts a valid form', () => {
    const errs = validateTaskForm({
      title: '完成练习册 P32',
      subject: 'math',
      estimated_minutes: 25,
      priority: 'high',
    });
    expect(hasErrors(errs)).toBe(false);
  });
});
