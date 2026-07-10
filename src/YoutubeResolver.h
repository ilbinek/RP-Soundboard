// src/YoutubeResolver.h
//----------------------------------
// RP Soundboard Source Code
// Copyright (c) 2015 Marius Graefe
// All rights reserved
// Contact: rp_soundboard@mgraefe.de
//----------------------------------

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class YoutubeResolver : public QObject
{
	Q_OBJECT

  public:
	explicit YoutubeResolver(QObject* parent = nullptr);
	~YoutubeResolver() override;

	static bool isYoutubeUrl(const QString& url);
	void resolve(const QString& url);
	void cancel();
	bool isBusy() const;

  signals:
	void progress(const QString& message);
	void finished(const QString& localPath, const QString& title);
	void failed(const QString& error);

  private slots:
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void onProcessError(QProcess::ProcessError error);
	void onProcessReadyRead();

  private:
	QString cacheDir() const;
	QString cachePathForUrl(const QString& url) const;
	QString findCachedFile(const QString& basePathWithoutExt) const;
	QString readCachedTitle(const QString& basePathWithoutExt) const;
	void writeCachedTitle(const QString& basePathWithoutExt, const QString& title) const;
	void startDownload(const QString& url, const QString& outputTemplate);
	void fetchTitleThenDownload(const QString& url);

	enum class Phase
	{
		Idle,
		FetchingTitle,
		Downloading,
	};

	QProcess* m_process;
	Phase m_phase;
	QString m_url;
	QString m_title;
	QString m_outputTemplate;
	QString m_basePath;
};
