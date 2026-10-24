document.querySelectorAll(".return-button[href='/']").forEach((button) => {
  button.addEventListener("click", (event) => {
    try {
      if (document.referrer && new URL(document.referrer).origin === window.location.origin && window.history.length > 1) {
        event.preventDefault();
        window.history.back();
      }
    } catch {
      // Keep the root link as a reliable fallback when the referrer is unavailable.
    }
  });
});
