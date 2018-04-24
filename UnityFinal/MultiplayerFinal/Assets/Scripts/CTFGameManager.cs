using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

public class CTFGameManager : NetworkBehaviour {

    public int m_numPlayers = 2;
    public const float m_gameTime = 30.0f;

    [SyncVar]
    public float currentTime = m_gameTime;

    public GameObject m_flag = null;
    public GameObject m_redPowerUp = null;
    public GameObject m_bluePowerUp = null;
    public Text timeText;

    public enum CTF_GameState
    {
        GS_WaitingForPlayers,
        GS_Ready,
        GS_InGame,
    }

    [SyncVar]
    CTF_GameState m_gameState = CTF_GameState.GS_WaitingForPlayers;

    public bool SpawnFlag()
    {
        GameObject flag = Instantiate(m_flag, new Vector3(0, 0, 0), new Quaternion());
        NetworkServer.Spawn(flag);
        return true;
    }

    void SpawnRedPowerup()
    {
        if(GameObject.FindGameObjectWithTag("RedPowerup") == null)
        {
            GameObject redPowerUp = Instantiate(m_redPowerUp, new Vector3(Random.Range(-20.0f, 20.0f), 0.0f, Random.Range(-20.0f, 20.0f)), new Quaternion());
            NetworkServer.Spawn(redPowerUp);
        }
        
    }

    void SpawnBluePowerup()
    {
        if (GameObject.FindGameObjectWithTag("BluePowerup") == null)
        {
            GameObject bluePowerUp = Instantiate(m_bluePowerUp, new Vector3(Random.Range(-20.0f, 20.0f), 0.0f, Random.Range(-20.0f, 20.0f)), new Quaternion());
            NetworkServer.Spawn(bluePowerUp);
        }

    }


    bool IsNumPlayersReached()
    {
        return CTFNetworkManager.singleton.numPlayers == m_numPlayers;
    }

	// Use this for initialization
	void Start () {
        timeText.text = "Time Remaining: " + currentTime;
    }
	
	// Update is called once per frame
	void Update ()
    {
	    if(isServer)
        {
            if(m_gameState == CTF_GameState.GS_WaitingForPlayers && IsNumPlayersReached())
            {
                m_gameState = CTF_GameState.GS_Ready;
            }
        }       
        UpdateGameState();
        if (m_gameState == CTF_GameState.GS_InGame && currentTime > 0)
        {
            currentTime -= Time.deltaTime;
            timeText.text = "Time Remaining: " + Mathf.Round(currentTime);
        }

        /*if(currentTime <= 0)
        {
            GameObject.FindGameObjectWithTag("Player").GetComponent<PlayerController>().checkScore();
        }*/
    }

    public void UpdateGameState()
    {
        if (m_gameState == CTF_GameState.GS_Ready)
        {
            //call whatever needs to be called
            if (isServer)
            {
                SpawnFlag();
                InvokeRepeating("SpawnRedPowerup", 5.0f, 15.0f);
                InvokeRepeating("SpawnBluePowerup", 5.0f, 15.0f);
                //change state to ingame
                
                

                m_gameState = CTF_GameState.GS_InGame;
            }
        }
        
    }
}
