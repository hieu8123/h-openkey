const button = document.querySelector("#copy-command");
const command = document.querySelector("#install-command");

button?.addEventListener("click", async () => {
  try {
    await navigator.clipboard.writeText(command.textContent.trim());
    button.textContent = "Đã sao chép";
    window.setTimeout(() => { button.textContent = "Sao chép"; }, 1800);
  } catch {
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents(command);
    selection.removeAllRanges();
    selection.addRange(range);
    button.textContent = "Đã chọn lệnh";
  }
});
