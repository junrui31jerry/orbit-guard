import "@testing-library/jest-dom/vitest";

import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";

import App from "./App";

vi.mock("./api", () => ({
  fetchStatus: () =>
    Promise.resolve({
      product: "OrbitGuard LEO Watch",
      data_mode: "snapshot",
      source: "CelesTrak public GP data",
      last_updated: "2026-05-29T00:00:00Z",
      prediction_window_hours: 72,
      active_risk_mode: "conservative",
      disclaimer: "Public-data close-approach screening, not official collision probability.",
    }),
  fetchSummary: () =>
    Promise.resolve({
      data_mode: "snapshot",
      last_updated: "2026-05-29T00:00:00Z",
      objects_screened: 2,
      active_satellites: 1,
      inactive_objects: 1,
      candidates: 1,
      critical: 0,
      warning: 1,
      watch: 0,
    }),
  fetchEvents: () => Promise.resolve([]),
}));

describe("App", () => {
  it("renders the public screening disclaimer", async () => {
    render(<App />);

    expect(await screen.findByText(/OrbitGuard LEO Watch/)).toBeInTheDocument();
    expect(await screen.findByText(/not official collision probability/)).toBeInTheDocument();
  });
});
