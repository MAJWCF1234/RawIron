(() => {
  const tabsEl = document.getElementById("tabs");
  const threadsEl = document.getElementById("threads");
  const tabBlurb = document.getElementById("tabBlurb");
  const emptyEl = document.getElementById("empty");
  const threadView = document.getElementById("threadView");
  const threadTitle = document.getElementById("threadTitle");
  const threadMeta = document.getElementById("threadMeta");
  const postsEl = document.getElementById("posts");
  const composer = document.getElementById("composer");
  const titleInput = document.getElementById("titleInput");
  const bodyInput = document.getElementById("bodyInput");
  const markInput = document.getElementById("markInput");
  const newThreadBtn = document.getElementById("newThreadBtn");
  const submitBtn = document.getElementById("submitBtn");

  let board = { tabs: [], threads: [] };
  let tabId = "floor";
  let threadId = "";
  let composingNew = false;

  const escapeHtml = (value) =>
    String(value || "")
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;");

  const stamp = (iso) => {
    if (!iso) return "";
    const date = new Date(iso);
    if (Number.isNaN(date.getTime())) return iso;
    return date.toISOString().slice(0, 16).replace("T", " ") + "Z";
  };

  const currentTab = () => board.tabs.find((tab) => tab.id === tabId) || board.tabs[0];

  const threadsInTab = () => board.threads.filter((thread) => thread.tab === tabId);

  const currentThread = () =>
    board.threads.find((thread) => thread.id === threadId && thread.tab === tabId);

  async function loadBoard() {
    const response = await fetch("/api/board", { cache: "no-store" });
    if (!response.ok) {
      throw new Error("board unavailable");
    }
    board = await response.json();
    if (!board.tabs.some((tab) => tab.id === tabId)) {
      tabId = board.tabs[0]?.id || "floor";
    }
    if (threadId && !currentThread()) {
      threadId = "";
    }
    render();
  }

  function renderTabs() {
    tabsEl.innerHTML = "";
    for (const tab of board.tabs) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = tab.title;
      button.className = tab.id === tabId ? "active" : "";
      button.addEventListener("click", () => {
        tabId = tab.id;
        threadId = "";
        composingNew = false;
        render();
      });
      tabsEl.appendChild(button);
    }
  }

  function renderThreads() {
    const tab = currentTab();
    tabBlurb.textContent = tab ? tab.blurb : "";
    threadsEl.innerHTML = "";
    const threads = threadsInTab();
    if (threads.length === 0) {
      const item = document.createElement("li");
      item.innerHTML = '<button type="button" disabled>No threads yet.</button>';
      threadsEl.appendChild(item);
      return;
    }
    for (const thread of threads) {
      const item = document.createElement("li");
      const button = document.createElement("button");
      button.type = "button";
      button.className = thread.id === threadId && !composingNew ? "active" : "";
      button.innerHTML = `${escapeHtml(thread.title)}<span class="count">${thread.posts.length} posts</span>`;
      button.addEventListener("click", () => {
        threadId = thread.id;
        composingNew = false;
        render();
      });
      item.appendChild(button);
      threadsEl.appendChild(item);
    }
  }

  function renderStage() {
    const thread = composingNew ? null : currentThread();
    emptyEl.hidden = Boolean(thread) || composingNew;
    threadView.hidden = !thread;
    composer.hidden = !(thread || composingNew);
    titleInput.hidden = !composingNew;
    titleInput.required = composingNew;
    submitBtn.textContent = composingNew ? "Start thread" : "Post";

    if (thread) {
      threadTitle.textContent = thread.title;
      threadMeta.textContent = `${thread.posts.length} posts · ${stamp(thread.created)}`;
      postsEl.innerHTML = "";
      for (const post of thread.posts) {
        const item = document.createElement("li");
        const who = post.mark ? post.mark : "anon";
        item.innerHTML = `<p class="who">${escapeHtml(who)} · ${escapeHtml(stamp(post.at))}</p><p class="body">${escapeHtml(post.body)}</p>`;
        postsEl.appendChild(item);
      }
      postsEl.scrollTop = postsEl.scrollHeight;
    }
  }

  function render() {
    renderTabs();
    renderThreads();
    renderStage();
  }

  newThreadBtn.addEventListener("click", () => {
    composingNew = true;
    threadId = "";
    titleInput.value = "";
    bodyInput.value = "";
    render();
    titleInput.focus();
  });

  composer.addEventListener("submit", async (event) => {
    event.preventDefault();
    const body = bodyInput.value.trim();
    const mark = markInput.value.trim();
    if (!body) {
      return;
    }
    const payload = { tab: tabId, body, mark };
    let route = "/api/posts";
    if (composingNew) {
      payload.title = titleInput.value.trim();
      if (!payload.title) {
        titleInput.focus();
        return;
      }
      route = "/api/threads";
    } else {
      payload.threadId = threadId;
    }
    submitBtn.disabled = true;
    try {
      const response = await fetch(route, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.error || "post failed");
      }
      bodyInput.value = "";
      titleInput.value = "";
      composingNew = false;
      threadId = data.thread.id;
      await loadBoard();
    } catch (error) {
      bodyInput.placeholder = String(error.message || error);
    } finally {
      submitBtn.disabled = false;
    }
  });

  loadBoard().catch((error) => {
    emptyEl.hidden = false;
    emptyEl.textContent = `Board is down. Start python server.py — ${error.message}`;
  });
})();
