"use strict";

const $ = (id) => document.getElementById(id);
let token = "";
let busy = false;
const message = (text) => { $("message").textContent = text; };

async function request(path, options = {}) {
  const response = await fetch(path, {
    ...options,
    credentials: "omit",
    cache: "no-store",
    headers: { Authorization: `Bearer ${token}` },
  });
  if (!response.ok) {
    if (response.status === 401) disconnect();
    throw new Error((await response.text()).trim() || `HTTP ${response.status}`);
  }
  return response;
}

async function refresh() {
  const area = $("area").value;
  const response = await request(`/api/${area}`);
  const items = await response.json();
  $("items").replaceChildren();
  items.sort((a, b) => a.name.localeCompare(b.name));
  for (const item of items) {
    const li = document.createElement("li");
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = "Download";
    button.onclick = async () => {
      button.disabled = true;
      try {
        const response = await request(`/api/${area}/${encodeURIComponent(item.name)}`);
        const url = URL.createObjectURL(await response.blob());
        const link = document.createElement("a");
        link.href = url;
        link.download = item.name;
        link.click();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
      } catch (error) { message(error.message); }
      finally { button.disabled = false; }
    };
    li.append(document.createTextNode(`${item.name} (${Math.ceil(item.size / 1024)} KiB) `), button);
    $("items").append(li);
  }
  message(items.length ? `${items.length} files` : "No files here yet.");
}

function disconnect() {
  token = "";
  $("token").value = "";
  $("library").hidden = true;
  $("connect").hidden = false;
  $("items").replaceChildren();
  message("Disconnected.");
}

$("connect").onsubmit = async (event) => {
  event.preventDefault();
  token = $("token").value.trim();
  $("token").value = "";
  try {
    await refresh();
    $("connect").hidden = true;
    $("library").hidden = false;
  } catch (error) { disconnect(); message(error.message); }
};

$("area").onchange = () => refresh().catch((error) => message(error.message));
$("refresh").onclick = () => refresh().catch((error) => message(error.message));
$("disconnect").onclick = disconnect;
$("send").onclick = async () => {
  if (busy) return;
  const files = Array.from($("upload").files);
  if (!files.length) { message("Choose at least one file."); return; }
  const area = $("area").value;
  busy = true;
  for (const id of ["send", "upload", "area", "refresh", "disconnect"]) $(id).disabled = true;
  try {
    for (const file of files) {
      message(`Uploading ${file.name}…`);
      await request(`/api/${area}/${encodeURIComponent(file.name)}`, { method: "PUT", body: file });
    }
    $("upload").value = "";
    await refresh();
    message(`Uploaded ${files.length} files.`);
  } catch (error) { message(error.message); }
  finally {
    busy = false;
    for (const id of ["send", "upload", "area", "refresh", "disconnect"]) $(id).disabled = false;
  }
};
