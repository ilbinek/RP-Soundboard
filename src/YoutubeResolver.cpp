// src/YoutubeResolver.cpp
//----------------------------------
// RP Soundboard Source Code
// Copyright (c) 2015 Marius Graefe
// All rights reserved
// Contact: rp_soundboard@mgraefe.de
//----------------------------------

#include "common.h"

#include "YoutubeResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include "ts3log.h"

namespace
{
const char* kYtDlp = "yt-dlp";
}


YoutubeResolver::YoutubeResolver(QObject* parent) :
	QObject(parent),
	m_process(new QProcess(this)),
	m_phase(Phase::Idle)
{
	connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
		&YoutubeResolver::onProcessFinished);
	connect(m_process, &QProcess::errorOccurred, this, &YoutubeResolver::onProcessError);
	connect(m_process, &QProcess::readyReadStandardOutput, this, &YoutubeResolver::onProcessReadyRead);
	connect(m_process, &QProcess::readyReadStandardError, this, &YoutubeResolver::onProcessReadyRead);
}


YoutubeResolver::~YoutubeResolver()
{
	cancel();
}


bool YoutubeResolver::isYoutubeUrl(const QString& url)
{
	const QString trimmed = url.trimmed();
	if (trimmed.isEmpty())
		return false;

	QUrl parsed(trimmed);
	if (!parsed.isValid())
		return false;

	const QString host = parsed.host().toLower();
	return host.contains("youtube.com") || host.contains("youtu.be") || host.contains("youtube-nocookie.com");
}


bool YoutubeResolver::isBusy() const
{
	return m_phase != Phase::Idle;
}


void YoutubeResolver::cancel()
{
	if (m_process->state() != QProcess::NotRunning)
	{
		m_process->kill();
		m_process->waitForFinished(3000);
	}
	m_phase = Phase::Idle;
	m_url.clear();
	m_title.clear();
	m_outputTemplate.clear();
	m_basePath.clear();
}


QString YoutubeResolver::cacheDir() const
{
	QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QDir dir(base);
	const QString sub = "rp_soundboard_yt";
	if (!dir.exists(sub))
		dir.mkpath(sub);
	return dir.filePath(sub);
}


QString YoutubeResolver::cachePathForUrl(const QString& url) const
{
	const QByteArray hash = QCryptographicHash::hash(url.trimmed().toUtf8(), QCryptographicHash::Sha1).toHex();
	return QDir(cacheDir()).filePath(QString::fromLatin1(hash));
}


QString YoutubeResolver::findCachedFile(const QString& basePathWithoutExt) const
{
	static const char* exts[] = {".mp3", ".m4a", ".opus", ".ogg", ".webm", ".wav", ".flac", nullptr};
	for (int i = 0; exts[i]; ++i)
	{
		const QString candidate = basePathWithoutExt + QLatin1String(exts[i]);
		if (QFileInfo::exists(candidate))
			return candidate;
	}
	return QString();
}


QString YoutubeResolver::readCachedTitle(const QString& basePathWithoutExt) const
{
	QFile file(basePathWithoutExt + ".title");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();
	return QString::fromUtf8(file.readAll()).trimmed();
}


void YoutubeResolver::writeCachedTitle(const QString& basePathWithoutExt, const QString& title) const
{
	QFile file(basePathWithoutExt + ".title");
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
		return;
	QTextStream out(&file);
	out.setCodec("UTF-8");
	out << title;
}


void YoutubeResolver::resolve(const QString& url)
{
	const QString trimmed = url.trimmed();
	if (!isYoutubeUrl(trimmed))
	{
		emit failed("Not a valid YouTube URL");
		return;
	}

	cancel();

	m_url = trimmed;
	m_basePath = cachePathForUrl(m_url);
	m_outputTemplate = m_basePath + ".%(ext)s";

	const QString cached = findCachedFile(m_basePath);
	if (!cached.isEmpty())
	{
		emit progress("Playing from cache…");
		QString title = readCachedTitle(m_basePath);
		if (title.isEmpty())
			title = QFileInfo(cached).completeBaseName();
		emit finished(cached, title);
		return;
	}

	emit progress("Fetching video info…");
	fetchTitleThenDownload(m_url);
}


void YoutubeResolver::fetchTitleThenDownload(const QString& url)
{
	m_phase = Phase::FetchingTitle;
	m_title.clear();
	m_process->setProgram(kYtDlp);
	m_process->setArguments(QStringList() << "--no-playlist"
										  << "--print"
										  << "title"
										  << url);
	m_process->setProcessChannelMode(QProcess::MergedChannels);
	m_process->start();
}


void YoutubeResolver::startDownload(const QString& url, const QString& outputTemplate)
{
	m_phase = Phase::Downloading;
	emit progress("Downloading audio…");
	m_process->setProgram(kYtDlp);
	m_process->setArguments(
		QStringList() << "--no-playlist"
					  << "-f"
					  << "ba/bestaudio"
					  << "-x"
					  << "--audio-format"
					  << "mp3"
					  << "--audio-quality"
					  << "0"
					  << "-o"
					  << outputTemplate << url
	);
	m_process->setProcessChannelMode(QProcess::MergedChannels);
	m_process->start();
}


void YoutubeResolver::onProcessReadyRead()
{
	const QByteArray data = m_process->readAll();
	if (data.isEmpty())
		return;

	const QString text = QString::fromUtf8(data).trimmed();
	if (text.isEmpty())
		return;

	if (m_phase == Phase::FetchingTitle)
	{
		// Title may arrive in chunks; keep last non-empty line
		const QStringList lines = text.split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts);
		if (!lines.isEmpty())
			m_title = lines.last().trimmed();
	}
	else if (m_phase == Phase::Downloading)
	{
		// Surface a short progress snippet from yt-dlp output
		const QStringList lines = text.split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts);
		for (const QString& line : lines)
		{
			if (line.contains('%') || line.startsWith("[download]", Qt::CaseInsensitive) ||
				line.startsWith("[ExtractAudio]", Qt::CaseInsensitive))
			{
				emit progress(line.left(80));
			}
		}
	}
}


void YoutubeResolver::onProcessError(QProcess::ProcessError error)
{
	if (error != QProcess::FailedToStart)
		return;

	// finished() may also fire; mark idle first so it is ignored
	const bool wasBusy = (m_phase != Phase::Idle);
	m_phase = Phase::Idle;
	if (wasBusy)
		emit failed("yt-dlp not found. Install yt-dlp and ensure it is on PATH.");
}


void YoutubeResolver::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if (m_phase == Phase::Idle)
		return;

	if (exitStatus != QProcess::NormalExit || exitCode != 0)
	{
		const Phase failedPhase = m_phase;
		m_phase = Phase::Idle;
		if (failedPhase == Phase::FetchingTitle)
		{
			// Title fetch failed — still try download without a title
			logInfo("yt-dlp title fetch failed (code %d); continuing with download", exitCode);
			m_title.clear();
			startDownload(m_url, m_outputTemplate);
			return;
		}
		emit failed(QString("yt-dlp failed (exit %1). Check the URL or update yt-dlp.").arg(exitCode));
		return;
	}

	if (m_phase == Phase::FetchingTitle)
	{
		startDownload(m_url, m_outputTemplate);
		return;
	}

	// Downloading finished
	m_phase = Phase::Idle;
	const QString cached = findCachedFile(m_basePath);
	if (cached.isEmpty())
	{
		emit failed("Download finished but audio file was not found in cache.");
		return;
	}

	QString title = m_title;
	if (title.isEmpty())
		title = QFileInfo(cached).completeBaseName();
	else
		writeCachedTitle(m_basePath, title);

	emit finished(cached, title);
}
