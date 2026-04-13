import { expect, test } from "@playwright/test";

test("UMD bundle runs a worker job in the browser", async ({ page }) => {
  await page.goto("/interfaces/js/test/browser/fixtures/worker.html");
  await expect(page.locator("#status")).toHaveText("ok");
  await expect(page.locator("#result")).toContainText("summary");
  await expect(page.locator("#result")).toContainText("1");
});
