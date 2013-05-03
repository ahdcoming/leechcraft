/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "storage.h"
#include <stdexcept>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <util/util.h>
#include <util/dblock.h>
#include "structures.h"
#include "oral.h"

namespace LeechCraft
{
namespace Poleemery
{
	Storage::Storage (QObject *parent)
	: QObject (parent)
	, DB_ (QSqlDatabase::addDatabase ("QSQLITE3", "Poleemery_Connection"))
	{
		const auto& dir = Util::CreateIfNotExists ("poleemeery");
		DB_.setDatabaseName (dir.absoluteFilePath ("database.db"));

		if (!DB_.open ())
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open database:"
					<< DB_.lastError ().text ();
			throw std::runtime_error ("Poleemery database creation failed");
		}

		InitializeTables ();
	}

	QList<Account> Storage::GetAccounts () const
	{
		return AccountInfo_.DoSelectAll_ ();
	}

	void Storage::AddAccount (const Account& acc)
	{
		AccountInfo_.DoInsert_ (acc);
	}

	QList<Entry> Storage::GetEntries (const Account& parent) const
	{
		return EntryInfo_.SelectByFKeysActor_ (boost::fusion::make_vector (parent));
	}

	void Storage::AddEntry (const Entry& entry)
	{
		EntryInfo_.DoInsert_ (entry);
	}

	namespace oral
	{
		template<>
		struct Type2Name<AccType>
		{
			QString operator() () const { return "TEXT"; }
		};

		template<>
		struct ToVariant<AccType>
		{
			QVariant operator() (const AccType& type) const
			{
				switch (type)
				{
				case AccType::BankAccount:
					return "BankAccount";
				case AccType::Cash:
					return "Cash";
				}
			}
		};

		template<>
		struct FromVariant<AccType>
		{
			AccType operator() (const QVariant& var) const
			{
				const auto& str = var.toString ();
				if (str == "BankAccount")
					return AccType::BankAccount;
				else
					return AccType::Cash;
			}
		};
	}

	void Storage::InitializeTables ()
	{
		AccountInfo_ = oral::Adapt<Account> (DB_);
		EntryInfo_ = oral::Adapt<Entry> (DB_);

		const auto& tables = DB_.tables ();

		QMap<QString, QString> queryCreates;
		queryCreates [Account::ClassName ()] = AccountInfo_.CreateTable_;
		queryCreates [Entry::ClassName ()] = EntryInfo_.CreateTable_;

		for (const auto& key : queryCreates.keys ())
			if (!tables.contains (key))
				QSqlQuery (DB_).exec (queryCreates [key]);
	}
}
}
