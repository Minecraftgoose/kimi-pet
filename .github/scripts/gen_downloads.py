#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 /downloads 页面的「历代下载」表格。

每次 CI push（含 tag push）跑一遍：从 GitHub Releases API 拉全部正式发行版，
把每个附件的 browser_download_url 直链写进表格，再用 contents API 写回
默认分支的 docs/downloads/index.html。

    python3 gen_downloads.py --template docs/downloads/index.html \
        --out /tmp/downloads.html --repo OWNER/REPO \
        [--latest-tag v1.4.6] [--releases-json FILE] [--publish] [--branch main]

--releases-json 供离线测试：喂一个 releases API 的数组 JSON，不联网。
--publish 时优先拿默认分支上现有的页面当模板，只替换标记区间，
这样 tag 打在旧 commit 上也不会把 main 上 newer 的页面样式覆盖掉。
内容没有变化就不产生空 commit。失败只打 ::warning::，不阻断构建。
"""
import argparse
import base64
import datetime as _dt
import html
import json
import os
import re
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
BEGIN_ROWS = "<!-- DOWNLOADS_ROWS_BEGIN -->"
END_ROWS = "<!-- DOWNLOADS_ROWS_END -->"
BEGIN_HERO = "<!-- DOWNLOADS_HERO_BEGIN -->"
END_HERO = "<!-- DOWNLOADS_HERO_END -->"
BEGIN_META = "<!-- DOWNLOADS_META_BEGIN -->"
END_META = "<!-- DOWNLOADS_META_END -->"
PATH = "docs/downloads/index.html"

WANT = ("KimiPet_x64.exe", "KimiPet_x86.exe",
        "KimiPetUpdater.bat", "SHA256SUMS.txt")


def warn(msg):
    print("::warning::" + msg.replace("\n", " "), file=sys.stderr)


def api(path, token=None):
    req = urllib.request.Request(API + path, headers={
        "Accept": "application/vnd.github+json",
        "User-Agent": "kimi-pet-downloads-page",
        **({"Authorization": "Bearer " + token} if token else {}),
    })
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.loads(r.read().decode("utf-8"))


def fetch_releases(repo, token):
    out, page = [], 1
    while page <= 10:
        batch = api("/repos/%s/releases?per_page=100&page=%d" % (repo, page), token)
        if not isinstance(batch, list):
            raise RuntimeError("releases API 返回的不是数组")
        out.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    return out


def fetch_latest(repo, token):
    try:
        return api("/repos/%s/releases/latest" % repo, token).get("tag_name")
    except Exception:
        return None


def esc(x):
    return html.escape(str(x), quote=True)


def size_text(n):
    try:
        n = int(n)
    except Exception:
        return ""
    if n >= 1024 * 1024:
        return "%.1f MB" % (n / 1048576.0)
    if n >= 1024:
        return "%d KB" % (n // 1024)
    return "%d B" % n


def file_link(asset, label=None):
    """一个直链 + 复制按钮。href 就是 browser_download_url 原值。"""
    url = asset.get("browser_download_url", "")
    name = asset.get("name", "")
    if not url:
        return "<span class=\"none\">—</span>"
    sz = size_text(asset.get("size", 0))
    txt = esc(label or name)
    return ('<span class="file"><a href="%s" title="%s">%s</a>'
            '<span class="sz">%s</span></span>'
            '<button type="button" class="copy" data-copy="%s" '
            'aria-label="复制 %s 的直链">复制</button>'
            % (esc(url), esc(url), txt, esc(sz), esc(url), esc(name)))


def cell(label, body):
    return '<td data-label="%s"><div class="cell">%s</div></td>' % (esc(label), body)


def build(releases, latest_tag):
    rows = []
    rels = [r for r in releases if not r.get("draft")]
    rels.sort(key=lambda r: r.get("published_at") or r.get("created_at") or "",
              reverse=True)
    if not latest_tag:
        for r in rels:
            if not r.get("prerelease"):
                latest_tag = r.get("tag_name")
                break

    for r in rels:
        tag = r.get("tag_name") or "?"
        date = (r.get("published_at") or r.get("created_at") or "")[:10] or "—"
        assets = {a.get("name", ""): a for a in r.get("assets", [])}
        x64, x86 = assets.get("KimiPet_x64.exe"), assets.get("KimiPet_x86.exe")
        zips = [a for n, a in assets.items() if n.lower().endswith(".zip")]

        ver = "<code>%s</code>" % esc(tag)
        if r.get("prerelease"):
            ver += '<span class="tag pre">预发布</span>'
        if latest_tag and tag == latest_tag:
            ver += '<span class="tag latest">Latest</span>'

        extra = []
        for n in ("KimiPetUpdater.bat", "SHA256SUMS.txt"):
            if n in assets:
                extra.append(file_link(assets[n]))
        for a in zips:
            extra.append(file_link(a))
        if not extra:
            extra.append('<span class="none">—</span>')
        note = ""
        if not x64 and not x86:
            note = '<span class="note">早期版本只提供 zip</span>'

        page = r.get("html_url", "")
        row = "      <tr>\n" + "\n".join([
            "        " + cell("版本", ver),
            "        " + cell("发布日期", esc(date)),
            "        " + cell("64 位", file_link(x64) if x64 else '<span class="none">—</span>'),
            "        " + cell("32 位", file_link(x86) if x86 else '<span class="none">—</span>'),
            "        " + cell("其它附件", "".join(extra) + note),
            "        " + cell("发行页",
                             '<a class="go" href="%s">查看</a>' % esc(page) if page else "—"),
        ]) + "\n      </tr>"
        rows.append(row)

    # 顶部「当前最新」卡片
    latest = next((r for r in rels if r.get("tag_name") == latest_tag), None)
    hero = []
    if latest:
        assets = {a.get("name", ""): a for a in latest.get("assets", [])}
        x64, x86 = assets.get("KimiPet_x64.exe"), assets.get("KimiPet_x86.exe")
        zips = [a for n, a in assets.items() if n.lower().endswith(".zip")]
        links = []
        if x64:
            links.append('<a class="btn primary" href="%s">下载 64 位</a>' %
                         esc(x64["browser_download_url"]))
        if x86:
            links.append('<a class="btn" href="%s">下载 32 位</a>' %
                         esc(x86["browser_download_url"]))
        if not links and zips:
            links.append('<a class="btn primary" href="%s">下载 zip</a>' %
                         esc(zips[0]["browser_download_url"]))
        if latest.get("html_url"):
            links.append('<a class="btn ghost" href="%s">发行说明</a>' %
                         esc(latest["html_url"]))
        hero = [
            '    <div class="latest-card">',
            '      <div class="latest-info">',
            '        <span class="tag latest">当前最新</span>',
            '        <h2>%s</h2>' % esc(latest.get("tag_name")),
            '        <p>%s · %d 个附件</p>' %
            (esc((latest.get("published_at") or "")[:10]), len(latest.get("assets", []))),
            '      </div>',
            '      <div class="latest-links">%s</div>' % "".join(links),
            '    </div>',
        ]
    meta = ("共 %d 个版本 · 最近同步 %s · 表格由 CI 在每次 push 时自动更新" %
            (len(rels), _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")))
    return "\n".join(hero), "\n".join(rows), meta


def replace_marked(text, begin, end, body):
    pat = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.S)
    if not pat.search(text):
        raise RuntimeError("模板缺标记 %s / %s" % (begin, end))
    return pat.sub(lambda m: begin + "\n" + body + "\n" + end, text, count=1)


def render(template, hero, rows, meta):
    out = replace_marked(template, BEGIN_HERO, END_HERO, hero)
    out = replace_marked(out, BEGIN_ROWS, END_ROWS, rows)
    out = replace_marked(out, BEGIN_META, END_META, meta)
    return out


def publish(repo, branch, token, content):
    """把结果写回默认分支；相同就不提交。返回 True 表示有变更。"""
    path = "/repos/%s/contents/%s?ref=%s" % (repo, PATH, branch)
    old_sha, old_text = None, None
    try:
        cur = api(path, token)
        old_sha = cur.get("sha")
        raw = cur.get("content", "")
        if raw:
            old_text = base64.b64decode(re.sub(r"\s+", "", raw)).decode("utf-8")
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise
    except Exception as e:
        raise RuntimeError("读取现有页面失败: %s" % e)

    # 远端页面若有标记，用它当模板 —— 保留 main 上可能更新的样式
    if old_text and BEGIN_ROWS in old_text and END_ROWS in old_text:
        hero, rows, meta = build(RELEASES_CACHE, LATEST_CACHE)
        content = render(old_text, hero, rows, meta)

    new_b64 = base64.b64encode(content.encode("utf-8")).decode("ascii")
    if old_text is not None:
        cur_b64 = base64.b64encode(old_text.encode("utf-8")).decode("ascii")
        if cur_b64 == new_b64:
            print("下载页无变化，跳过提交")
            return False

    payload = {
        "message": "docs: 同步历代下载页 [skip ci]",
        "content": new_b64,
        "branch": branch,
    }
    if old_sha:
        payload["sha"] = old_sha
    req = urllib.request.Request(
        API + "/repos/%s/contents/%s" % (repo, PATH),
        data=json.dumps(payload).encode("utf-8"),
        method="PUT",
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "kimi-pet-downloads-page",
            "Authorization": "Bearer " + token,
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        resp = json.loads(r.read().decode("utf-8"))
    print("已写回 %s@%s：%s" % (PATH, branch, resp.get("commit", {}).get("sha", "?")))
    return True


RELEASES_CACHE = []
LATEST_CACHE = None


def main():
    global RELEASES_CACHE, LATEST_CACHE
    ap = argparse.ArgumentParser()
    ap.add_argument("--template", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    ap.add_argument("--branch", default=os.environ.get("DOCS_BRANCH", "main"))
    ap.add_argument("--latest-tag", default="")
    ap.add_argument("--releases-json", default="")
    ap.add_argument("--publish", action="store_true")
    args = ap.parse_args()

    try:
        if args.releases_json:
            rels = json.load(open(args.releases_json, encoding="utf-8"))
            latest = args.latest_tag or None
        else:
            if not args.repo:
                raise RuntimeError("缺 --repo 或 GITHUB_REPOSITORY")
            token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
            rels = fetch_releases(args.repo, token)
            latest = args.latest_tag or fetch_latest(args.repo, token)
        RELEASES_CACHE, LATEST_CACHE = rels, latest
        tpl = open(args.template, encoding="utf-8").read()
        hero, rows, meta = build(rels, latest)
        content = render(tpl, hero, rows, meta)
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        print("生成 %s：%d 个版本，latest=%s" % (args.out, len(rels), latest or "(无)"))
    except Exception as e:
        warn("生成下载页失败：%s" % e)
        return 0  # 文档同步不阻断构建

    if args.publish:
        try:
            token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
            if not token:
                raise RuntimeError("缺 GH_TOKEN")
            publish(args.repo, args.branch, token, content)
        except Exception as e:
            warn("写回下载页失败：%s" % e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
