import "@testing-library/jest-dom/vitest";

import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import EarthView from "./EarthView";

describe("EarthView", () => {
  it("mounts the 3D Earth container", () => {
    render(<EarthView />);

    expect(screen.getByLabelText("3D Earth event view")).toBeInTheDocument();
  });
});
