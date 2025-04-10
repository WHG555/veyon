/*
 * AccessControlPage.cpp - implementation of the access control page
 *
 * Copyright (c) 2016-2025 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QInputDialog>
#include <QMessageBox>

#include "AccessControlPage.h"
#include "AccessControlRule.h"
#include "VeyonCore.h"
#include "VeyonConfiguration.h"
#include "AccessControlProvider.h"
#include "Configuration/UiMapping.h"
#include "AccessControlRuleEditDialog.h"
#include "UserGroupsBackendManager.h"

#include "ui_AccessControlPage.h"

AccessControlPage::AccessControlPage( QWidget* parent ) :
	ConfigurationPage( parent ),
	ui(new Ui::AccessControlPage),
	m_accessControlRulesModel( this ),
	m_accessControlRulesTestDialog( this )
{
	ui->setupUi(this);

	ui->accessControlRulesView->setModel( &m_accessControlRulesModel );

	updateAccessGroupsLists();
	updateAccessControlRules();
}



AccessControlPage::~AccessControlPage()
{
	delete ui;
}



void AccessControlPage::resetWidgets()
{
	FOREACH_VEYON_ACCESS_CONTROL_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY);

	if (VeyonCore::config().isAccessControlRulesProcessingEnabled() == false &&
		VeyonCore::config().isAccessRestrictedToUserGroups() == false)
	{
		ui->skipAccessControl->setChecked(true);
	}

	m_accessGroups = VeyonCore::config().authorizedUserGroups();
	auto cleanedUpAccessGroups = m_accessGroups;

	const auto availableGroups = VeyonCore::userGroupsBackendManager().configuredBackend()->
								 userGroups(VeyonCore::config().useDomainUserGroups());

	const auto accessGroupNotAvailable = [&availableGroups](const QString& group) {
		return availableGroups.contains(group) == false;
	};
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	cleanedUpAccessGroups.removeIf(accessGroupNotAvailable);
#else
	cleanedUpAccessGroups.erase(std::remove_if(cleanedUpAccessGroups.begin(), cleanedUpAccessGroups.end(), accessGroupNotAvailable), cleanedUpAccessGroups.end());
#endif
	if (m_accessGroups != cleanedUpAccessGroups)
	{
		m_accessGroups = cleanedUpAccessGroups;
		VeyonCore::config().setAuthorizedUserGroups(m_accessGroups);
	}

	updateAccessGroupsLists();
	updateAccessControlRules();
}



void AccessControlPage::connectWidgetsToProperties()
{
	FOREACH_VEYON_ACCESS_CONTROL_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY)
}



void AccessControlPage::applyConfiguration()
{
	resetWidgets();
}



void AccessControlPage::addAccessGroup()
{
	const auto items = ui->allGroupsList->selectedItems();

	for( auto item : items )
	{
		m_accessGroups.removeAll( item->text() );
		m_accessGroups += item->text();
	}

	VeyonCore::config().setAuthorizedUserGroups( m_accessGroups );

	updateAccessGroupsLists();
}



void AccessControlPage::removeAccessGroup()
{
	const auto items = ui->accessGroupsList->selectedItems();

	for( auto item : items )
	{
		m_accessGroups.removeAll( item->text() );
	}

	VeyonCore::config().setAuthorizedUserGroups( m_accessGroups );

	updateAccessGroupsLists();
}



void AccessControlPage::updateAccessGroupsLists()
{
	ui->accessGroupsList->setUpdatesEnabled( false );

	ui->allGroupsList->clear();
	ui->accessGroupsList->clear();

	VeyonCore::userGroupsBackendManager().reloadConfiguration();

	const auto groups = VeyonCore::userGroupsBackendManager().configuredBackend()->
						userGroups(VeyonCore::config().useDomainUserGroups());

	for( const auto& group : groups )
	{
		if( m_accessGroups.contains( group ) )
		{
			ui->accessGroupsList->addItem( group );
		}
		else
		{
			ui->allGroupsList->addItem( group );
		}
	}

	ui->accessGroupsList->setUpdatesEnabled( true );
}


void AccessControlPage::addAccessControlRule()
{
	AccessControlRule newRule;
	newRule.setAction( AccessControlRule::Action::Allow );

	if( AccessControlRuleEditDialog( newRule, this ).exec() )
	{
		VeyonCore::config().setAccessControlRules( VeyonCore::config().accessControlRules() << newRule.toJson() );

		updateAccessControlRules();
	}
}



void AccessControlPage::removeAccessControlRule()
{
	QJsonArray accessControlRules = VeyonCore::config().accessControlRules();

	accessControlRules.removeAt( ui->accessControlRulesView->currentIndex().row() );

	VeyonCore::config().setAccessControlRules( accessControlRules );

	updateAccessControlRules();
}



void AccessControlPage::editAccessControlRule()
{
	QJsonArray accessControlRules = VeyonCore::config().accessControlRules();

	int row = ui->accessControlRulesView->currentIndex().row();

	if( row >= accessControlRules.count() )
	{
		return;
	}

	AccessControlRule rule( accessControlRules[row] );

	if( AccessControlRuleEditDialog( rule, this ).exec() )
	{
		accessControlRules.replace( row, rule.toJson() );

		modifyAccessControlRules( accessControlRules, row );
	}
}



void AccessControlPage::moveAccessControlRuleDown()
{
	QJsonArray accessControlRules = VeyonCore::config().accessControlRules();

	int row = ui->accessControlRulesView->currentIndex().row();
	int newRow = row + 1;

	if( newRow < accessControlRules.size() )
	{
		const QJsonValue swapValue = accessControlRules[row];
		accessControlRules[row] = accessControlRules[newRow];
		accessControlRules[newRow] = swapValue;

		modifyAccessControlRules( accessControlRules, newRow );
	}
}



void AccessControlPage::moveAccessControlRuleUp()
{
	QJsonArray accessControlRules = VeyonCore::config().accessControlRules();

	int row = ui->accessControlRulesView->currentIndex().row();
	int newRow = row - 1;

	if( newRow >= 0 )
	{
		const QJsonValue swapValue = accessControlRules[row];
		accessControlRules[row] = accessControlRules[newRow];
		accessControlRules[newRow] = swapValue;

		modifyAccessControlRules( accessControlRules, newRow );
	}
}



void AccessControlPage::testUserGroupsAccessControl()
{
	const auto username = QInputDialog::getText( this, tr( "Enter username" ),
												 tr( "Please enter a user login name whose access permissions to test:" ) );

	if (username.isEmpty())
	{
		return;
	}

	if( AccessControlProvider().processAuthorizedGroups( username ) )
	{
		QMessageBox::information( this, tr( "Access allowed" ),
								  tr( "The specified user is allowed to access computers with this configuration." ) );
	}
	else
	{
		QMessageBox::warning( this, tr( "Access denied" ),
								  tr( "The specified user is not allowed to access computers with this configuration." ) );
	}
}



void AccessControlPage::testAccessControlRules()
{
	m_accessControlRulesTestDialog.exec();
}



void AccessControlPage::modifyAccessControlRules(const QJsonArray &accessControlRules, int selectedRow)
{
	VeyonCore::config().setAccessControlRules( accessControlRules );

	updateAccessControlRules();

	ui->accessControlRulesView->setCurrentIndex( m_accessControlRulesModel.index( selectedRow ) );
}



void AccessControlPage::updateAccessControlRules()
{
	m_accessControlRulesModel.reload();
}
